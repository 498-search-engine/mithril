document.addEventListener('DOMContentLoaded', function() {
    const searchForm = document.getElementById('search-form');
    const searchInput = document.getElementById('search-input');
    const loadingIndicator = document.getElementById('loading');
    const resultsMeta = document.getElementById('results-meta');
    const resultsCount = document.getElementById('results-count');
    const queryTime = document.getElementById('query-time');
    const resultsContainer = document.getElementById('results');
    const exampleLinks = document.querySelectorAll('.example');
    
    // Mock data for demonstration
    const mockResults = [
      {
        title: "Optimizing C++ Performance for Modern Processors",
        url: "https://example.com/cpp-optimization",
        snippet: "A comprehensive guide to optimizing C++ code for modern processors. Covers memory layout, cache coherence, and SIMD instructions."
      },
      {
        title: "Data Structures in Concurrent Programming",
        url: "https://example.com/concurrent-data-structures",
        snippet: "Implementation techniques for thread-safe data structures without compromising performance. Includes lock-free algorithms and memory ordering."
      },
      {
        title: "Efficient Algorithms for Graph Processing",
        url: "https://example.com/graph-algorithms",
        snippet: "Deep dive into algorithms for processing large-scale graphs efficiently. Includes path finding, minimum spanning trees, and network flow."
      },
      {
        title: "Understanding Memory Models in C++",
        url: "https://example.com/cpp-memory-models",
        snippet: "Explaining memory models, atomics, and thread synchronization primitives in modern C++. Essential for writing correct concurrent code."
      }
    ];
  
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
    
    function performSearch(query) {
      // Show loading state
      loadingIndicator.style.display = 'block';
      resultsMeta.style.display = 'none';
      resultsContainer.innerHTML = '';
      
      // Simulate search delay (very short to imply performance)
      setTimeout(() => {
        // Hide loading indicator
        loadingIndicator.style.display = 'none';
        
        // Display results metadata
        const resultCount = mockResults.length;
        const searchTime = (Math.random() * 0.008 + 0.001).toFixed(3);
        
        resultsCount.textContent = `${resultCount} results`;
        queryTime.textContent = `${searchTime}s`;
        resultsMeta.style.display = 'flex';
        
        // Display results
        mockResults.forEach(result => {
          const resultElement = document.createElement('div');
          resultElement.className = 'result';
          
          resultElement.innerHTML = `
            <h2><a href="${result.url}">${result.title}</a></h2>
            <div class="result-url">${result.url}</div>
            <p class="result-snippet">${result.snippet}</p>
          `;
          
          resultsContainer.appendChild(resultElement);
        });
      }, 200);
    }
  });