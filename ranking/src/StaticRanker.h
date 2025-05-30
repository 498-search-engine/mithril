#ifndef RANKING_STATICRANKER_H
#define RANKING_STATICRANKER_H

#include "core/config.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

/**
* @brief This is different from CrawlerRanker in terms of scoring. I'm decoupling it so that we can make more specific
tunes to static ranking for what we'd want for queries (but not necessarily for crawling/differences do not matter as
much then).

* Primary differentiating is scoring. The scoring is more granular and we scale up at different points. This is because
we want to be able to tune the score more easily and not have to worry about the penalty being too high or too low. We
can also add more scoring rules if we want to.

* We no longer use a % system as well, just a points based system.
*/

namespace mithril::ranking {
static inline core::Config Config = core::Config("staticranker.conf");

static inline const int32_t BaseScore = 1000;

// * HTTPS
static inline const int32_t HttpsScore = Config.GetInt("HttpsScore");

// * Site TLD (whitelist)
static inline const int32_t WhitelistTldScore = Config.GetInt("WhitelistTldScore");

// * Domain whitelist
static inline const int32_t WhitelistDomainScore = Config.GetInt("WhitelistDomainScore");

// * Domain name length
// note: includes TLD length
// note: ignores www.
// note: ignored if domain name is in whitelist
static inline const int32_t DomainNameScore = Config.GetInt("DomainNameScore");
static inline const int32_t DomainLengthAcceptable = Config.GetInt("DomainLengthAcceptable");
static inline const int32_t DomainPenaltyPerExtraLength = Config.GetInt("DomainPenaltyPerExtraLength");

// * URL length
// note: URL length does not include domain name length
static inline const int32_t UrlLengthScore = Config.GetInt("UrlLengthScore");
static inline const int32_t UrlLengthAcceptable = Config.GetInt("UrlLengthAcceptable");
static inline const int32_t UrlPenaltyPerExtraLength = Config.GetInt("UrlPenaltyPerExtraLength");

// * Number of parameters
static inline const int32_t NumberParamScore = Config.GetInt("NumberParamScore");
static inline const int32_t NumberParamAcceptable = Config.GetInt("NumberParamAcceptable");
static inline const int32_t NumberParamPenaltyPerExtraParam = Config.GetInt("NumberParamPenaltyPerExtraParam");

// * Depth of page
static inline const int32_t DepthPageScore = Config.GetInt("DepthPageScore");
static inline const int32_t DepthPageAcceptable = Config.GetInt("DepthPageAcceptable");
static inline const int32_t DepthPagePenalty = Config.GetInt("DepthPagePenalty");

/**
    Positive ranking
*/
// * Extensions
const std::unordered_set<std::string> GoodExtensionList = {"asp", "html", "htm", "php", "asp", ""};
static inline const int32_t ExtensionBoost = Config.GetInt("ExtensionBoost");

/**
     Negative ranking
*/
// * Subdomain count
// Penalty is per subdomain that is not `www`
// www.example.com has 1 subdomains (example)
// www.eecs.example.com has 2 subdomain (eecs, example)
// only penalized if the domain is not in the whitelist (e.g en.wikipedia.org)
static inline const int32_t SubdomainAcceptable = Config.GetInt("SubdomainAcceptable");
static inline const int32_t SubdomainPenalty = Config.GetInt("SubdomainPenalty");
// * Any number in domain name
// only penalized if the domain is not in the whitelist
static inline const int32_t DomainNameNumberPenalty = Config.GetInt("DomainNameNumberPenalty");

// * Numbers of length > 4 (e.g not years) in URL (this is after the domain name)
static inline const int32_t URLNumberPenalty = Config.GetInt("URLNumberPenalty");

// Normalization Details
static inline const double MaxPossibleScore = BaseScore + HttpsScore + WhitelistTldScore + WhitelistDomainScore +
                                               DomainNameScore + UrlLengthScore + NumberParamScore + DepthPageScore +
                                               ExtensionBoost;
static inline const double MinScore = BaseScore - 500; // Assuming a domain will not get penalized more than 500 points (if it does, it's probably really bad anyways)
static inline const double DiffScore = MaxPossibleScore - MinScore;

struct StaticRankingsStruct {
    std::string tld;
    std::string domainName;
    std::string extension;
    int32_t urlLength = 0;
    int32_t parameterCount = 0;
    int32_t pageDepth = 0;
    int32_t subdomainCount = 0;
    bool numberInDomainName = false;
    bool numberInURL = false;
    bool isHttps = false;
};

const std::unordered_set<std::string> WhitelistTld = {
    "com",  // Commercial (most trusted and widely used)
    "co",
    "org",  // Organizations (non-profits, open-source projects, etc.)
    "net",  // Network infrastructure (widely trusted)
    "edu",  // Educational institutions (highly trusted)
    "gov",  // U.S. government entities (highly trusted)
    "int",  // International organizations (e.g., NATO, UN)
};

const std::unordered_set<std::string> WhitelistDomain = {
    // News and Media
    "bbc.com",             // British Broadcasting Corporation (global news)
    "nytimes.com",         // The New York Times (US and international news)
    "theguardian.com",     // The Guardian (UK and global news)
    "reuters.com",         // Reuters (global news and financial reporting)
    "apnews.com",          // Associated Press (fact-based news reporting)
    "aljazeera.com",       // Al Jazeera (Middle Eastern and global news)
    "npr.org",             // National Public Radio (US news and culture)
    "wsj.com",             // The Wall Street Journal (business and financial news)
    "washingtonpost.com",  // The Washington Post (US and global news)
    "bloomberg.com",       // Bloomberg (business and financial news)

    // Education and Reference
    "en.wikipedia.org",
    "wikipedia.org",            // Wikipedia (crowdsourced encyclopedia)
    "britannica.com",           // Encyclopaedia Britannica (authoritative reference)
    "khanacademy.org",          // Khan Academy (educational resources)
    "ted.com",                  // TED Talks (educational and inspirational talks)
    "edx.org",                  // edX (online courses from universities)
    "coursera.org",             // Coursera (online courses and certifications)
    "scholar.google.com",       // Google Scholar (academic research)
    "jstor.org",                // JSTOR (academic journals and books)
    "arxiv.org",                // arXiv (preprint research papers in STEM fields)
    "pubmed.ncbi.nlm.nih.gov",  // PubMed (biomedical research)

    // Government and Public Information
    "usa.gov",        // US Government Services and Information
    "gov.uk",         // UK Government Services and Information
    "who.int",        // World Health Organization (global health information)
    "cdc.gov",        // Centers for Disease Control and Prevention (US health information)
    "nasa.gov",       // NASA (space and science information)
    "nsa.gov",        // National Science Foundation (scientific research)
    "data.gov",       // US Government Open Data
    "europa.eu",      // European Union Official Website
    "un.org",         // United Nations (global issues and policies)
    "worldbank.org",  // World Bank (global development data)

    // Science and Technology
    "nature.com",         // Nature (scientific research and news)
    "sciencemag.org",     // Science Magazine (scientific research)
    "ieee.org",           // IEEE (technology and engineering resources)
    "techcrunch.com",     // TechCrunch (technology news and startups)
    "wired.com",          // Wired (technology and culture)
    "arstechnica.com",    // Ars Technica (technology and science news)
    "mit.edu",            // MIT (research and educational resources)
    "stackoverflow.com",  // Stack Overflow (programming and developer community)
    "github.com",         // GitHub (open-source projects and code)
    "nist.gov",           // National Institute of Standards and Technology (technology standards)

    // Health and Medicine
    "mayoclinic.org",       // Mayo Clinic (health information and advice)
    "webmd.com",            // WebMD (health information and tools)
    "nih.gov",              // National Institutes of Health (US health research)
    "healthline.com",       // Healthline (health and wellness information)
    "medlineplus.gov",      // MedlinePlus (health information from the NIH)
    "clevelandclinic.org",  // Cleveland Clinic (health information)
    "hopkinsmedicine.org",  // Johns Hopkins Medicine (health resources)
    "psychologytoday.com",  // Psychology Today (mental health resources)

    // Business and Finance
    "forbes.com",        // Forbes (business and financial news)
    "cnbc.com",          // CNBC (business and financial news)
    "ft.com",            // Financial Times (global financial news)
    "economist.com",     // The Economist (global business and economics)
    "marketwatch.com",   // MarketWatch (financial markets and news)
    "fool.com",          // The Motley Fool (investment advice)
    "sec.gov",           // US Securities and Exchange Commission (financial regulations)
    "investopedia.com",  // Investopedia (financial education)

    // General Knowledge and Culture
    "nationalgeographic.com",  // National Geographic (science, history, and culture)
    "smithsonianmag.com",      // Smithsonian Magazine (history, science, and culture)
    "history.com",             // History Channel (historical information)
    "time.com",                // TIME Magazine (news and culture)
    "britishmuseum.org",       // British Museum (cultural and historical resources)
    "loc.gov",                 // Library of Congress (historical and cultural archives)
    "tate.org.uk",             // Tate (art and culture)
    "metmuseum.org",           // The Metropolitan Museum of Art (art and culture)
    "imdb.com",                // IMDb (movies and entertainment)
    "goodreads.com",           // Goodreads (books and literature)

    // Technology and Computing
    "microsoft.com",          // Microsoft (technology and software)
    "apple.com",              // Apple (technology and products)
    "google.com",             // Google (search and technology)
    "mozilla.org",            // Mozilla (open-source software and web standards)
    "linuxfoundation.org",    // Linux Foundation (open-source software)
    "python.org",             // Python (programming language)
    "developer.android.com",  // Android Developer (mobile development)
    "aws.amazon.com",         // Amazon Web Services (cloud computing)
    "docker.com",             // Docker (containerization and DevOps)
    "git-scm.com",            // Git (version control system)
};

/*
 * Gets all relevant ranking info in one pass of the URL string.
 */
void GetStringRankings(std::string_view url, StaticRankingsStruct& ranker);
double GetUrlStaticRank(std::string_view url);
}  // namespace mithril::ranking

#endif
