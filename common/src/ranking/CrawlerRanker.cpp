#include "ranking/CrawlerRanker.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace mithril::ranking {

uint32_t GetUrlRank(std::string_view url) {
    CrawlerRankingsStruct ranker{
        .tld = "", 
        .domainName = "",
        .extension = "",
        .urlLength = 0, 
        .parameterCount = 0,
        .pageDepth = 0, 
        .subdomainCount = 0,
        .isHttps = false
    };

    GetStringRankings(url, ranker);

    uint32_t score = 0;

    // * Site TLD (whitelist)
    if (WhitelistTld.contains(ranker.tld)) {
        score += WhitelistTldScore;
    }

    // * Domain whitelist
    if (WhitelistDomain.contains(ranker.domainName)) {
        score += WhitelistDomainScore;
    }

    // * Domain name length
    uint32_t domainNamePenalty = 0;
    if (ranker.domainName.length() > DomainLengthAcceptable) {
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        domainNamePenalty = DomainPenaltyPerExtraLength * (ranker.domainName.length() - DomainLengthAcceptable);
    }
    score += DomainNameScore - std::min(domainNamePenalty, DomainNameScore);

    // * URL length
    uint32_t urlPenalty = 0;
    if (ranker.urlLength > UrlLengthAcceptable) {
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        urlPenalty = UrlPenaltyPerExtraLength * (ranker.urlLength - UrlLengthAcceptable);
    }
    score += UrlLengthScore - std::min(urlPenalty, UrlLengthScore);

    // * Number of parameters
    uint32_t numParamPenalty = 0;
    if (ranker.parameterCount > NumberParamAcceptable) {
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        numParamPenalty = NumberParamPenaltyPerExtraParam * (ranker.parameterCount - NumberParamAcceptable);
    }
    score += NumberParamScore - std::min(numParamPenalty, NumberParamScore);

    // Depth of page
    uint32_t depthPagePenalty = 0;
    if (ranker.pageDepth > DepthPageAcceptable) {
        // NOLINTNEXTLINE(bugprone-narrowing-conversions)
        depthPagePenalty = DepthPagePenalty * (ranker.pageDepth - DepthPageAcceptable);
    }
    score += DepthPageScore - std::min(depthPagePenalty, DepthPageScore);

    // * HTTPS
    if (!ranker.isHttps) {
        score -= std::min(score, HttpsDebuffScore);
    }

    // * Extensions
    if (GoodExtensionList.contains(ranker.extension)) {
        score += ExtensionBoost;
    } else if (BadExtensionList.contains(ranker.extension)) {
        score -= ExtensionDebuff;
    }

    // Subdomain count
    if (ranker.subdomainCount > SubdomainAcceptable) {
        score -= SubdomainPenalty * (ranker.subdomainCount - SubdomainAcceptable);
    }

    return score;
}

void GetStringRankings(std::string_view url, CrawlerRankingsStruct& ranker) {
    const char* c = url.data();
    const char* end = url.data() + url.size();

    // Read until end of protocol to check whether it is HTTPS or not
    while (*c != ':') {
        if (*c == 's') {
            ranker.isHttps = true;
        }
        c++;
    };

    // Get rid of two slashes after : (//)
    c = c + 3;

    bool readTld = false;
    // Read until the domain part is over
    while (*c != '/' && c < end) {
        if (readTld) {
            ranker.tld.push_back(*c);
        }

        if (*c == '.') {
            readTld = true;
            ranker.tld = "";
            ranker.subdomainCount++;
        }

        ranker.domainName.push_back(*c);
        c++;
    }

    if (ranker.domainName.starts_with("www.")) {
        ranker.domainName.erase(0, 4);
        ranker.subdomainCount--;
    }

    // Read until the URL is over
    bool readExtension = false;
    while (c < end) {
        if (*c == '?' || *c == '&') {
            ranker.parameterCount++;
            readExtension = false;
        } else if (*c == '/') {
            ranker.pageDepth++;
        } else if (*c == '.') {
            readExtension = true;
            ranker.extension += *c;
        }

        ranker.urlLength++;
        c++;
    }

    // if it ended in a /, depth should not increase
    if (c[-1] == '/') {
        ranker.pageDepth--;
    }
}

}  // namespace mithril::ranking
