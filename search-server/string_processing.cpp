#include "string_processing.h"

using namespace std;
vector<string_view> SplitIntoWords(string_view text) {
	string_view str = text;
	vector<string_view> words;
	while (true) {
		size_t space = str.find(' ');
		words.push_back(str.substr(0, space));
		if (space == str.npos) {
			break;
		}
		else {
			str.remove_prefix(space + 1);
		}
	}
	return words;
}