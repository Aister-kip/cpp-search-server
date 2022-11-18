#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server)
    , no_result_num_(0)
    , current_time_(0)
{
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    const std::vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
    AddRequest(result.size());
    return result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    const std::vector<Document> result = search_server_.FindTopDocuments(raw_query);
    AddRequest(result.size());
    return result;
}

int RequestQueue::GetNoResultRequests() const {
    return no_result_num_;
}

void RequestQueue::AddRequest(int results_num) {
    ++current_time_;
    while (!requests_.empty() && sec_in_day_ <= current_time_ - requests_.front().incoming_time) {
        if (requests_.front().result == 0) {
            --no_result_num_;
        }
        requests_.pop_front();
    }
    requests_.push_back({ current_time_, results_num });
    if (results_num == 0) {
        ++no_result_num_;
    }
}