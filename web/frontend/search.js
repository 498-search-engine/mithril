document.addEventListener('DOMContentLoaded', function() {
    const searchForm = document.getElementById('search-form');
    const searchInput = document.getElementById('search-input');
    const loadingIndicator = document.getElementById('loading');
    const resultsMeta = document.getElementById('results-meta');
    const resultsCount = document.getElementById('results-count');
    const queryTime = document.getElementById('query-time');
    const resultsContainer = document.getElementById('results');
    const exampleLinks = document.querySelectorAll('.example');

    function strip(html) {
        let doc = new DOMParser().parseFromString(html, 'text/html');
        return doc.body.textContent || "";
    }
     
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
        const urlParams = new URLSearchParams(window.location.search);
        const cachedResults = queryCache.get(query);
        if (!localStorage.getItem('nocache') && cachedResults) {
            displayResults(cachedResults, true);
            return;
        }

        if (query == 'nocache') {
            localStorage.setItem('nocache', '1');
            console.log('Disabled cache on this device');
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

    function loadSnippetsForVisibleResults() {
        const results = document.querySelectorAll('.result');
        if (results.length === 0) return;
        
        const visibleDocIds = [];
        results.forEach(result => {
            const docId = result.getAttribute('data-doc-id');
            if (docId) {
                visibleDocIds.push(docId);
            }
        });

        if (visibleDocIds.length === 0) return;

        const query = document.getElementById('search-input').value;
        const url = `/api/snippets?ids=${visibleDocIds.join(',')}&q=${encodeURIComponent(query)}`;

        // Create a new fetch request
        fetch(url)
            .then(response => {
                // Check if response is ok
                if (!response.ok) {
                    throw new Error(`HTTP error! Status: ${response.status}`);
                }

                // Get a reader from the response body stream
                const reader = response.body.getReader();
                const decoder = new TextDecoder();
                let buffer = '';

                // Function to process the stream chunks
                function processChunks() {
                    return reader.read().then(({ done, value }) => {
                        // If the stream is done, return
                        if (done) return;

                        // Decode the chunk and add it to our buffer
                        buffer += decoder.decode(value, { stream: true });

                        // Process any complete JSON objects in the buffer
                        let jsonEndIndex;
                        while ((jsonEndIndex = buffer.indexOf('}')) !== -1) {
                            try {
                                // Extract and parse a JSON object
                                const jsonStr = buffer.substring(0, jsonEndIndex + 1);
                                const snippetData = JSON.parse(jsonStr);

                                // Update the snippet in the DOM
                                const docId = Object.keys(snippetData)[0];
                                const snippet = snippetData[docId];
                                updateSnippetForDoc(docId, snippet);

                                // Remove the processed JSON from the buffer
                                buffer = buffer.substring(jsonEndIndex + 1);
                            } catch (e) {
                                // If we can't parse the JSON yet, wait for more data
                                break;
                            }
                        }

                        // Continue processing chunks
                        return processChunks();
                    });
                }

                // Start processing the stream
                return processChunks();
            })
            .catch(error => {
                console.error('Error loading snippets:', error);
            });
    }

    function updateSnippetForDoc(docId, snippet) {
        const resultElement = document.querySelector(`.result[data-doc-id="${docId}"]`);
        if (resultElement) {
            const snippetElement = resultElement.querySelector('.result-snippet');
            if (snippetElement) {
                snippetElement.innerHTML = highlightQueryTerms(snippet, document.getElementById('search-input').value);
            }
        }
    }
    
    function highlightQueryTerms(snippet, query) {
        const terms = query.toLowerCase()
            .split(/\s+/)
            .filter(term => term.length > 2 && term !== 'and' && term !== 'or' && term !== 'not');
            
        let result = snippet;
        
        for (const term of terms) {
            const regex = new RegExp(`(\\b${escapeRegExp(term)}\\b)`, 'gi');
            result = result.replace(regex, '<span class="highlight">$1</span>');
        }
        
        return result;
    }
    
    function escapeRegExp(string) {
        return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }
    
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
                resultElement.setAttribute('data-doc-id', result.id);
                const domain = new URL(result.url).origin;
                const faviconUrl = `https://www.google.com/s2/favicons?sz=64&domain_url=${domain}`;

                resultElement.innerHTML = `
                    <h2><a href="${result.url}" target="_blank">${strip(result.title)}</a></h2>
                    <div class="result-url" style="display: flex; align-items: center; gap: 6px; margin: 4px 0;">
                        <img src="${faviconUrl}" alt="favicon" class="favicon" style="width: 16px; height: 16px;">
                        <span class="result-url-text">${result.url}</span>
                    </div>
                    <p class="result-snippet">${result.snippet}</p>
                `;

                resultsContainer.appendChild(resultElement);
            });
            setTimeout(loadSnippetsForVisibleResults, 50);
        } else {
            resultsContainer.innerHTML = '<div class="no-results">No results found</div>';
        }
    }

});