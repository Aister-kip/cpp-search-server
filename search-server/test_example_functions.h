#pragma once
#include "search_server.h"

void FindTopDocuments(const SearchServer& search_server, const std::string_view raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string_view query);
void AddDocument(SearchServer& server, int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);