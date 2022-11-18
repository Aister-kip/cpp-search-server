#include "test_example_functions.h"

void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query) {
    using namespace std;
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            cout << document;
        }
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const std::string_view query) {
    using namespace std;
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (auto document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

void AddDocument(SearchServer& server, int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    server.AddDocument(document_id, document, status, ratings);
}