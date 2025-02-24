#include "CrawlerRanker.h"

namespace mithril::crawler_ranker {

uint32_t GetUrlRank(const std::string& url) {
    CrawlerRankingsStruct ranker{
        .tld = "", .domainName = "", .urlLength = 0, .parameterCount = 0, .pageDepth = 0, .isHttps = false};

    crawler_ranker::GetStringRankings(url.data(), ranker);

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

    return score;
}

void GetStringRankings(const char* url, CrawlerRankingsStruct& ranker) {
    // Read until end of protocol to check whether it is HTTPS or not
    while (*url != ':') {
        if (*url == 's') {
            ranker.isHttps = true;
        }
        url++;
    };

    // Get rid of two slashes after : (//)
    url = url + 3;

    bool readTld = false;
    // Read until the domain part is over
    while (*url != '/') {
        if (readTld) {
            ranker.tld.push_back(*url);
        }

        if (*url == '.') {
            readTld = true;
            ranker.tld = "";
        }

        ranker.domainName.push_back(*url);
        url++;
    }

    if (ranker.domainName.starts_with("www.")) {
        ranker.domainName.erase(0, 4);
    }

    // Read until the URL is over
    while (*url) {
        if (*url == '?' || *url == '&') {
            ranker.parameterCount++;
        } else if (*url == '/') {
            ranker.pageDepth++;
        }

        ranker.urlLength++;
        url++;
    }

    url++;
    
    // if it ended in a /, depth should not increase
    if (*(url--) == '/') {
        ranker.pageDepth--;
    }
}
}  // namespace mithril::crawler_ranker