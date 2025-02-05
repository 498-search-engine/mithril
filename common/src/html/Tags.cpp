#include "html/Tags.h"

#include <cassert>
#include <cstring>
#include <ctype.h>

namespace mithril::html::internal {

// name points to beginning of the possible HTML tag name.
// nameEnd points to one past last character.
// Comparison is case-insensitive.
// Use a binary search.
// If the name is found in the TagsRecognized table, return
// the corresponding action.
// If the name is not found, return OrdinaryText.

DesiredAction LookupPossibleTag(const char* name, const char* nameEnd) {
    if (!nameEnd)
        nameEnd = name + strlen(name);

    int left = 0;
    int right = NumberOfTags - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;

        const char* t1 = name;
        const char* t2 = TagsRecognized[mid].Tag;
        int strCmpRes = 0;

        while (t1 < nameEnd && *t2 && !strCmpRes) {
            strCmpRes = tolower(*t1) - tolower(*t2);
            t1++;
            t2++;
        }

        if (!strCmpRes && t1 == nameEnd && !*t2)
            return TagsRecognized[mid].Action;

        if (strCmpRes < 0 || (strCmpRes == 0 && t1 == nameEnd))
            right = mid - 1;
        else
            left = mid + 1;
    }

    return DesiredAction::OrdinaryText;
}

}  // namespace mithril::html::internal
