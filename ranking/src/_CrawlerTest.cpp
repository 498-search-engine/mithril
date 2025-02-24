#include "CrawlerRanker.h"
#include <queue>
#include <iostream>

int main() {
    std::string realURLs[] = {
        // 1–10
        "https://www.google.com/search?q=chatgpt",
        "https://www.youtube.com/watch?v=dQw4w9WgXcQ",
        "https://en.wikipedia.org/wiki/Artificial_intelligence",
        "https://www.reddit.com/r/programming",
        "https://www.amazon.com/s?k=headphones",
        "https://twitter.com/search?q=%23AI",
        "https://www.facebook.com/Meta/",
        "https://github.com/topics/machine-learning",
        "https://medium.com/tag/cybersecurity",
        "https://www.linkedin.com/in/satyanadella",
      
        // 11–20
        "https://www.netflix.com/browse",
        "https://www.paypal.com/signin",
        "https://www.nytimes.com/section/technology",
        "https://www.bbc.co.uk/news",
        "https://www.cnn.com/world",
        "https://www.bloomberg.com/markets",
        "https://www.reuters.com/search/news?query=renewable+energy",
        "https://www.wsj.com/?mod=wsjheader_logo",
        "https://github.blog/",
        "https://docs.python.org/3/",
      
        // 21–30
        "https://stackoverflow.com/questions/tagged/javascript",
        "https://en.gravatar.com/",
        "https://wordpress.org/themes/",
        "https://www.w3schools.com/html/default.asp",
        "https://www.mozilla.org/en-US/firefox/",
        "https://slack.com/intl/en-in/",
        "https://drive.google.com/",
        "https://www.dropbox.com/",
        "https://www.spotify.com/us/",
        "https://soundcloud.com/",
      
        // 31–40
        "https://www.imdb.com/chart/top/",
        "https://www.rottentomatoes.com/",
        "https://www.apple.com/itunes/",
        "https://developer.android.com/",
        "https://www.uber.com/global/en/",
        "https://www.airbnb.com/s/Hawaii/homes",
        "https://www.booking.com/",
        "https://www.tripadvisor.com/Tourism-g28932-Hawaii-Vacations.html",
        "https://maps.google.com/",
        "https://www.openstreetmap.org/",
      
        // 41–50
        "https://www.ibm.com/cloud",
        "https://www.oracle.com/cloud/",
        "https://azure.microsoft.com/en-us/",
        "https://cloud.google.com/",
        "https://aws.amazon.com/",
        "https://www.digitalocean.com/",
        "https://www.linode.com/",
        "https://vercel.com/",
        "https://firebase.google.com/",
        "https://about.me/",
      
        // 51–60
        "https://www.nasa.gov/",
        "https://www.who.int/",
        "https://www.un.org/",
        "https://openai.com/",
        "https://chat.openai.com/",
        "https://www.bbc.com/news/technology",
        "https://www.theguardian.com/uk",
        "https://www.economist.com/",
        "https://www.forbes.com/",
        "https://techcrunch.com/",
      
        // 61–70
        "https://www.ted.com/topics/technology",
        "https://www.edx.org/",
        "https://www.coursera.org/",
        "https://www.udemy.com/courses/search/?src=ukw&q=python",
        "https://www.khanacademy.org/",
        "https://scratch.mit.edu/",
        "https://www.wikipedia.org/wiki/Main_Page",
        "https://www.gnu.org/licenses/gpl-3.0.en.html",
        "https://archive.org/",
        "https://arxiv.org/",
        
        "http://hohoh.org",
        // 71–80
        "https://github.com/",
        "https://gitlab.com/",
        "https://bitbucket.org/",
        "https://stackoverflow.com/",
        "https://www.atlassian.com/",
        "https://aws.amazon.com/s3/",
        "https://azure.microsoft.com/en-us/services/storage/",
        "https://cloud.google.com/storage/",
        "https://www.npmjs.com/",
        "https://yarnpkg.com/",
      
        // 81–90
        "https://pypi.org/",
        "https://rubygems.org/",
        "https://packagist.org/",
        "https://www.godaddy.com/",
        "https://www.namecheap.com/",
        "https://www.cloudflare.com/",
        "https://about.gitlab.com/",
        "https://www.youtube.com/feed/trending",
        "https://www.twitch.tv/directory",
        "https://discord.com/channels/@me",
      
        // 91–100
        "https://telegram.org/",
        "https://www.whatsapp.com/",
        "https://www.instagram.com/",
        "https://dribbble.com/",
        "https://www.behance.net/",
        "https://www.pinterest.com/",
        "https://www.flickr.com/",
        "https://unsplash.com/",
        "https://pixabay.com/",
        "https://www.canva.com/create/"
    };

    std::priority_queue<std::pair<uint32_t, std::string>> crawlerQueue;
    for (const std::string& url : realURLs) {
        uint32_t ranking = mithril::crawler_ranker::GetUrlRank(url);
        crawlerQueue.push({ranking, url});
    }

    size_t i = 1;
    while (!crawlerQueue.empty()) {
        const auto &p = crawlerQueue.top();
        
        std::cout << i << ". " << p.second << " - " << p.first << std::endl;
        crawlerQueue.pop();

        i++;
    }
}