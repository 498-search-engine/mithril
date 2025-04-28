query
NOT query

50
1269

30
1279

38
1281  





while (1):
    socket code
    get the query 
        broadcast here
        keep checking when counter == k for queue[0]
        aggregate and send bac


k threads (number of indexes)
 - current_query = [ (query, false)   ]
    - mutex and a cv

    - will broadast to all the threads to wakeup 
    - the queue has an element
    - execute query -> query engine logic
    - then do ranking logic 
    - put results inot designated vector
    - then udpate counter by 1






worker.conf
    index path 1
    index path 2
    ...


We should contain a structure to store like index related data


read in worker.con 

setup index data
setup threads 

struct SingleIndexData:
    std::strign index path
    QueryEngine class for each of these
    results []

map: 
    indexpath -> vector<results>


each thread could have an id 
          t1     t2     t3     t4  ...   tn
array [   Status     Status    Status     Status        Status  ]

Status:
    true/false 


aggregation: 
- k sorted arrays and we need to aggregate 
- O(n log k)
- We do need all of the 


queue: [(query, atomic_counter)]


while (1):
    socket code
    get the query 
    update the queue
    continue its work 
    

manager thread: 

while(1):
    check the queue
    broadcast here
    keep checking when counter == k for queue[0]

    then we can send back to client 



while (1):
    socket code
    get the query 
        broadcast here
        keep checking when counter == k for queue[0]
        aggregate and send bac


k threads (number of indexes)
 - current_query = [ (query, false)   ]
    - mutex and a cv

    - will broadast to all the threads to wakeup 
    - the queue has an element
    - execute query -> query engine logic
    - then do ranking logic 
    - put results inot designated vector
    - then udpate counter by 1





 




query 2: 














# Setup
## General
`mkdir build`

`cmake -DCMAKE_BUILD_TYPE=Release -S . -B build`

`cmake --build build`

`git submodule update --init --recursive`

`cd build`

## Crawl (for a few minutes)
`mkdir data`

`mkdir data/snapshot`

`mkdir data/docs`

`mkdir data/state`

`./crawler/mithril_crawler ../crawler/crawler.conf`

control-C when you want to stop 

## Build Index
`./index/mithril_indexer data`

## Run Query Engine
There are many scripts here, you can find them all in `build/query`, however I will only cover how to set up the general coordinator and workers and other important scripts

`/query/parser-driver`: super helpful for testing single queries

Go to `query/src/servers.conf` where we define the mithril worker server ip and ports

Run: 

`./query/mithril_worker --index index_output --port <some_port>`

make sure the port lines up with what is in `servers.conf`
In another terminal, do: 

`./query/mithril_coordinator --conf ../query/servers.conf`

Then you can send queries from the coordinator to the worker! 




# Other



./query/mithril_coordinator --conf ../query/servers.conf 

<!-- Second Pass -->

# Simpler Query Language
Based on Bleve

## Basic Grammar


Title terms get a "#" prefix
URL terms get a "@" prefix
Anchor terms get a "$" prefix
Description terms get a "%" prefix
Body terms have no special prefix

```cpp
enum class FieldType {
    BODY = 0,
    TITLE = 1,
    URL = 2,
    ANCHOR = 3,
    DESC = 4
    // Can be extended with HEADING, BOLD, etc.
};
```

phrase := term [ operator phrase ]

term := field_expr | simple_term | quoted_term | grouped_expr

field_expr := field ":" (simple_term | quoted_term)

field := "TITLE" | "TEXT"   // Enum of searchable fields

simple_term := <string of alphanumeric characters>

quoted_term := "\"" <string> "\""   // Exact phrase matching

grouped_expr := "(" phrase ")"   // For logical grouping

operator := "SPACE" | "AND" | "OR" | "NOT"     // Space is the default


operators and fields are searchable if they do not fit our grammar



Wildcards (like * for prefix matching)
Range queries (for numeric or date fields)
Proximity operators (NEAR, WITHIN n words)
Boosting specific terms with ^ or similar notation



--------------------------------



## Examples

- `database`                      // Search for "database" in all fields
- `TITLE:database`                // Search for "database" in title field only
- `"database systems"`            // Exact phrase match in all fields
- `TITLE:"database systems"`      // Exact phrase match in title field only
- `database AND systems`          // Both terms must appear
- `database OR systems`           // Either term can appear
- `database NOT mysql`            // "database" must appear, "mysql" must not
- `(database OR systems) AND sql` // Logical grouping with AND/OR

# Potential query language (advanced first pass)

query         ::= clause (WS clause)* ;

clause        ::= modifier? expression boost? ;

modifier      ::= "+" | "-" ;

boost         ::= "^" number ;

expression    ::= scoped_expr | grouped_expr | term_expr ;

scoped_expr   ::= field ":" (regex_expr | phrase_expr | range_expr | term_expr) ;

grouped_expr  ::= "(" query ")" ;

term_expr     ::= word ;

phrase_expr   ::= QUOTE phrase QUOTE ;

regex_expr    ::= "/" regex "/" ;

range_expr    ::= range_operator value ;

range_operator ::= ">" | ">=" | "<" | "<=" ;

field         ::= identifier ;

word          ::= escaped_word ;

phrase        ::= (escaped_word | WS)* ;

regex         ::= (escaped_regex_char | any_char_except_slash)+ ;

value         ::= number | quoted_date ;

quoted_date   ::= QUOTE date_string QUOTE ;

escaped_word         ::= (ESCAPED_CHAR | NON_SPECIAL_CHAR)+ ;
escaped_regex_char   ::= ESCAPED_CHAR ;
any_char_except_slash ::= any_char - "/" ;

identifier    ::= (ALPHA | DIGIT | "_")+ ;

number        ::= DIGIT+ ("." DIGIT+)? ;

date_string   ::= DIGIT DIGIT DIGIT DIGIT "-" DIGIT DIGIT "-" DIGIT DIGIT ;

ESCAPED_CHAR        ::= "\\" SPECIAL_CHAR ;
SPECIAL_CHAR        ::= "+" | "-" | "=" | "&" | "|" | ">" | "<" | "!" | "(" | ")" | "{" | "}" | "[" | "]" | "^" | "\"" | "~" | "*" | "?" | ":" | "\\" | "/" | WS ;
NON_SPECIAL_CHAR    ::= any_char - SPECIAL_CHAR ;
QUOTE               ::= "\"" ;
WS                  ::= " " | "\t" | "\n" | "\r" ;
ALPHA               ::= "a".."z" | "A".."Z" ;
DIGIT               ::= "0".."9" ;




uct DocInfo {
//     uint32_t doc_id;
//     uint32_t total_frequency;  // Sum of all term frequencies
//     std::string url;
//     std::vector<std::string> title;
//     uint32_t frequency; 
    
//     // Store term-specific information
//     struct TermData {
//         std::string term;
//         uint32_t frequency;
//         std::vector<uint32_t> positions;
        
//         TermData(std::string t, uint32_t freq) : term(std::move(t)), frequency(freq) {}
//     };
    
//     std::vector<TermData> term_data;  // Data for each term that matched this document

//     DocInfo(uint32_t id, uint32_t freq) : doc_id(id), total_frequency(freq) {}
    
//     // Add information for a new term
//     void addTerm(const std::string& term, uint32_t freq, const std::vector<uint32_t>& pos = {}) {
//         TermData data(term, freq);
//         data.positions = pos;
//         term_data.push_back(std::move(data));
//         total_frequency += freq;
//     }
    
//     // Merge another DocInfo into this one
//     void merge(const DocInfo& other) {
//         total_frequency += other.total_frequency;
        
//         // Merge term data
//         for (const auto& data : other.term_data) {
//             term_data.push_back(data);
//         }
        
//         // If the other document has URL/title and this one doesn't, copy it
//         if (url.empty() && !other.url.empty()) {
//             url = other.url;
//         }
        
//         if (title.empty() && !other.title.empty()) {
//             title = other.title;
//         }
//     }
// };