document.addEventListener('DOMContentLoaded', function() {
    const searchForm = document.getElementById('search-form');
    const searchInput = document.getElementById('search-input');
    const loadingIndicator = document.getElementById('loading');
    const resultsMeta = document.getElementById('results-meta');
    const resultsCount = document.getElementById('results-count');
    const queryTime = document.getElementById('query-time');
    const resultsContainer = document.getElementById('results');
    const exampleLinks = document.querySelectorAll('.example');

    // Client-side cache setup
    const queryCache = {
        data: {},
        maxSize: 20,
        ttl: 5 * 60 * 1000, // 5 minutes in milliseconds

        set: function(query, results) {
            // Implement LRU - remove oldest entry if at capacity
            const keys = Object.keys(this.data);
            if (keys.length >= this.maxSize) {
                let oldestKey = keys[0];
                let oldestTime = this.data[oldestKey].timestamp;

                for (const key of keys) {
                    if (this.data[key].timestamp < oldestTime) {
                        oldestTime = this.data[key].timestamp;
                        oldestKey = key;
                    }
                }

                delete this.data[oldestKey];
            }

            this.data[query] = {
                results: results,
                timestamp: Date.now()
            };

            // Store in localStorage for persistence
            try {
                const cacheData = JSON.stringify({
                    data: this.data,
                    updated: Date.now()
                });
                localStorage.setItem('mithrilSearchCache', cacheData);
            } catch (e) {
                console.warn('Failed to save cache to localStorage:', e);
            }
        },

        get: function(query) {
            const entry = this.data[query];
            if (!entry) return null;

            // Check if expired
            if (Date.now() - entry.timestamp > this.ttl) {
                delete this.data[query];
                return null;
            }

            return entry.results;
        },

        loadFromStorage: function() {
            try {
                const storedCache = localStorage.getItem('mithrilSearchCache');
                if (storedCache) {
                    const parsed = JSON.parse(storedCache);
                    // Only use if less than 1 hour old
                    if (Date.now() - parsed.updated < 60 * 60 * 1000) {
                        this.data = parsed.data;

                        // Clean expired entries
                        for (const query in this.data) {
                            if (Date.now() - this.data[query].timestamp > this.ttl) {
                                delete this.data[query];
                            }
                        }
                    }
                }
            } catch (e) {
                console.warn('Failed to load cache from localStorage:', e);
                this.data = {};
            }
        }
    };

    // Load cache from localStorage on startup
    queryCache.loadFromStorage();

    // Handle form submission
    searchForm.addEventListener('submit', function(e) {
        e.preventDefault();
        const query = searchInput.value.trim();

        if (!query) return;

        performSearch(query);
    });

    // Handle example link clicks
    exampleLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            const query = this.getAttribute('data-query');
            searchInput.value = query;
            performSearch(query);
        });
    });

    // Debounce function
    function debounce(func, wait) {
        let timeout;
        return function(...args) {
            const context = this;
            clearTimeout(timeout);
            timeout = setTimeout(() => func.apply(context, args), wait);
        };
    }

    // Perform search with debouncing and caching
    const performSearch = debounce(function(query) {
        // Show loading state
        loadingIndicator.style.display = 'block';
        resultsMeta.style.display = 'none';
        resultsContainer.innerHTML = '';

        // Check client-side cache first
        const cachedResults = queryCache.get(query);
        if (cachedResults) {
            displayResults(cachedResults, true);
            return;
        }

        // Fetch search results from API
        fetch(`/api/search?q=${encodeURIComponent(query)}&max=50`)
            .then(response => {
                if (!response.ok) {
                    throw new Error('Search request failed with status ' + response.status);
                }
                return response.json();
            })
            .then(data => {
                // Cache the results
                queryCache.set(query, data);

                // Display results
                displayResults(data, false);
            })
            .catch(error => {
                console.error('Search error:', error);
                loadingIndicator.style.display = 'none';
                resultsContainer.innerHTML = `
                  <div class="error">
                      <p>Search error: ${error.message}</p>
                      <p>Please try again later.</p>
                  </div>`;
            });
    }, 300);

    // Display results function
    function displayResults(data, fromCache) {
        // Hide loading indicator
        loadingIndicator.style.display = 'none';

        // Display results metadata
        const resultCount = data.total || 0;
        const searchTime = ((data.time_ms || 0) / 1000).toFixed(3);

        resultsCount.textContent = `${resultCount} results`;
        queryTime.textContent = `${searchTime}s${fromCache ? ' (cached)' : ''}`;
        resultsMeta.style.display = 'flex';

        // FIXED: Remove any existing demo indicators first
        const existingIndicators = resultsMeta.querySelectorAll('.demo-indicator');
        existingIndicators.forEach(indicator => indicator.remove());

        // Show demo mode indicator if applicable
        if (data.demo_mode || data.fallback) {
            const demoIndicator = document.createElement('div');
            demoIndicator.className = 'demo-indicator';
            demoIndicator.textContent = data.fallback ? 'Fallback results' : 'Demo mode';
            resultsMeta.appendChild(demoIndicator);
        }

        if (data.results && data.results.length > 0) {
            data.results.forEach(result => {
                const resultElement = document.createElement('div');
                resultElement.className = 'result';

                resultElement.innerHTML = `
                  <h2><a href="${result.url}" target="_blank">${result.title}</a></h2>
                  <div class="result-url">${result.url}</div>
                  <p class="result-snippet">${result.snippet}</p>
              `;

                resultsContainer.appendChild(resultElement);
            });
        } else {
            resultsContainer.innerHTML = '<div class="no-results">No results found</div>';
        }
    }
});