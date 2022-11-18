#include "process_queries.h"
#include <execution>

using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const vector<string>& queries) {

    vector<vector<Document>> result(queries.size());
    transform(execution::par, 
        queries.begin(), 
        queries.end(), 
        result.begin(), 
        [&search_server](const string element) { return search_server.FindTopDocuments(execution::par, element); });
    return result;
}

vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    vector<Document> result;
    for (const auto& document : ProcessQueries(search_server, queries)) {
        result.insert(result.end(), document.begin(), document.end());
    }
    return result;
}