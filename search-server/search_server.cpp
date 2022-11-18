#include "search_server.h"

#include <cmath>

using namespace std;


SearchServer::SearchServer(const string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status,
    const vector<int>& ratings) {

    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const auto& word : words) {
        word_to_document_freqs_[string(word)][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][string(word)] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, status);
}

vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query);
}

vector<Document> SearchServer::FindTopDocuments(const execution::sequenced_policy&, const string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

vector<Document> SearchServer::FindTopDocuments(const execution::parallel_policy&, const string_view raw_query) const {
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() const {
    return document_ids_.end();
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(execution::seq, document_id);
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) {
    if (document_ids_.count(document_id) != 0) {
        documents_.erase(document_id);
        document_ids_.erase(document_id);
        for (auto& [_, freq] : word_to_document_freqs_) {
            if (freq.count(document_id) != 0) {
                freq.erase(document_id);
            }
        }
        document_to_word_freqs_.erase(document_id);
    }
}

void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) {
    if (document_ids_.count(document_id) != 0) {
        documents_.erase(document_id);
        document_ids_.erase(document_id);
        for_each(execution::par, word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
            [document_id](auto& document) {document.second.erase(document_id); });
        document_to_word_freqs_.erase(document_id);
    }

}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string_view, double> dummy;
    if (document_to_word_freqs_.count(document_id) != 0) {
        for (auto [word, freq] : document_to_word_freqs_.at(document_id)) {
            dummy.emplace(string_view(word), freq);
        }
    }
    return dummy;
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::sequenced_policy&, string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    const auto status = documents_.at(document_id).status;

    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word.data()) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word.data()).count(document_id)) {
            return { {}, status };
        }
    }

    vector<string_view> matched_words;
    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(string(word)) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string(word)).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    return { matched_words, status };
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const execution::parallel_policy&, string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query, true);

    const auto status = documents_.at(document_id).status;

    const auto word_checker =
        [this, document_id](const string_view word) {
        const auto it = word_to_document_freqs_.find(string(word));
        return it != word_to_document_freqs_.end() && it->second.count(document_id);
    };

    if (any_of(execution::par, query.minus_words.begin(), query.minus_words.end(), word_checker)) {
        return { {}, status };
    }

    vector<string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(
        execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        word_checker
    );
    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return { matched_words, status };
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(string(word)) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + text.data() + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const string_view text, bool skip_sort) const {
    Query result;
    for (const string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if (!skip_sort) {
        for (auto* words : { &result.plus_words, &result.minus_words }) {
            sort(words->begin(), words->end());
            words->erase(unique(words->begin(), words->end()), words->end());
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(string(word)).size());
}