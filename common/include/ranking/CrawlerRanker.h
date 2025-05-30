#ifndef COMMON_RANKING_CRAWLERRANKER_H
#define COMMON_RANKING_CRAWLERRANKER_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

namespace mithril::ranking {
// * HTTPS (50%)
// a debuff for sites that don't have HTTPS by 50 points.
constexpr int32_t HttpsDebuffScore = 50;
// * Site TLD (whitelist) (10%)
constexpr int32_t WhitelistTldScore = 10;
// * Domain whitelist (10%)
constexpr int32_t WhitelistDomainScore = 10;
// * Domain name length (10%)
// note: includes TLD length
// note: ignores www.
// note: ignored if domain name is in whitelist
constexpr int32_t DomainNameScore = 10;
constexpr int32_t DomainLengthAcceptable = 11;
constexpr int32_t DomainPenaltyPerExtraLength = 5;
// * URL length (10%)
// note: URL length does not include domain name length
constexpr int32_t UrlLengthScore = 10;
constexpr int32_t UrlLengthAcceptable = 60;
constexpr int32_t UrlPenaltyPerExtraLength = 5;
// * Number of parameters (20%)
constexpr int32_t NumberParamScore = 20;
constexpr int32_t NumberParamAcceptable = 1;
constexpr int32_t NumberParamPenaltyPerExtraParam = 5;
// * Depth of page (40%)
constexpr int32_t DepthPageScore = 40;
constexpr int32_t DepthPageAcceptable = 2;
constexpr int32_t DepthPagePenalty = 15;

/**
    Positive ranking
*/
// * Extensions (+30%)
const std::unordered_set<std::string> GoodExtensionList = {"asp", "html", "htm", "php", "asp", ""};
constexpr int32_t ExtensionBoost = 30;

/**
     Negative ranking
*/
// * Subdomain count (-50%)
// Penalty is per subdomain that is not `www`
// www.example.com has 1 subdomains (example)
// www.eecs.example.com has 2 subdomain (eecs, example)
// only penalized if the domain is not in the whitelist (e.g en.wikipedia.org)
constexpr int32_t SubdomainAcceptable = 1;
constexpr int32_t SubdomainPenalty = 15;

// * Extensions (Does not crawl if found)
const std::unordered_set<std::string> BadExtensionList = {
    "pdf", "doc", "docx", "ppt",  "pptx", "xls",  "xlsx", "odt", "ods",    "odp",    "zip", "rar", "7z",
    "tar", "gz",  "bz2",  "exe",  "dmg",  "pkg",  "deb",  "rpm", "iso",    "img",    "msi", "apk", "bin",
    "dat", "csv", "tsv",  "json", "xml",  "sql",  "db",   "mdb", "sqlite", "log",    "bak", "tmp", "swp",
    "gif", "svg", "webp", "ico",  "bmp",  "tiff", "psd",  "ai",  "eps",    "mp3",    "wav", "ogg", "flac",
    "aac", "wma", "mid",  "mp4",  "avi",  "mov",  "wmv",  "flv", "mkv",    "webm",   "m4v", "3gp", "mpeg",
    "mpg", "m4a", "aiff", "au",   "raw",  "cr2",  "nef",  "orf", "sr2",    "torrent"};

// * Any number in domain name (-20%)
// only penalized if the domain is not in the whitelist
constexpr int32_t DomainNameNumberPenalty = 20;

// * Numbers of length > 4 (e.g not years) in URL (-35%) (this is after the domain name)
constexpr int32_t URLNumberPenalty = 35;

struct CrawlerRankingsStruct {
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

int32_t GetUrlRank(std::string_view url);

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
void GetStringRankings(std::string_view url, CrawlerRankingsStruct& ranker);

}  // namespace mithril::ranking

#endif
