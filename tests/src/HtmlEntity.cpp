#include "html/Entity.h"

#include <string>
#include <gtest/gtest.h>

using namespace mithril::html;

// Test that valid named entities are correctly decoded
TEST(DecodeHtmlEntityTest, ValidNamedEntities) {
    // Test &amp;
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&amp;", out));
        EXPECT_EQ(out, "Start: &");
    }

    // Test &lt;
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&lt;", out));
        EXPECT_EQ(out, "Start: <");
    }

    // Test &gt;
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&gt;", out));
        EXPECT_EQ(out, "Start: >");
    }

    // Test &quot;
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&quot;", out));
        EXPECT_EQ(out, "Start: \"");
    }

    // Test &apos;
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&apos;", out));
        EXPECT_EQ(out, "Start: '");
    }

    // Test &nbsp;
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&nbsp;", out));
        EXPECT_EQ(out, "Start:  ");
    }
}

// Test that valid decimal entities are correctly decoded
TEST(DecodeHtmlEntityTest, ValidDecimalEntities) {
    // Test &#38; (ampersand)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#38;", out));
        EXPECT_EQ(out, "Start: &");
    }

    // Test &#60; (less than)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#60;", out));
        EXPECT_EQ(out, "Start: <");
    }

    // Test &#62; (greater than)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#62;", out));
        EXPECT_EQ(out, "Start: >");
    }

    // Test &#34; (quotation mark)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#34;", out));
        EXPECT_EQ(out, "Start: \"");
    }

    // Test &#39; (apostrophe)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#39;", out));
        EXPECT_EQ(out, "Start: '");
    }
}

// Test that valid hexadecimal entities are correctly decoded
TEST(DecodeHtmlEntityTest, ValidHexEntities) {
    // Test &#x26; (ampersand)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x26;", out));
        EXPECT_EQ(out, "Start: &");
    }

    // Test &#x3C; (less than)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x3C;", out));
        EXPECT_EQ(out, "Start: <");
    }

    // Test &#x3E; (greater than)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x3E;", out));
        EXPECT_EQ(out, "Start: >");
    }

    // Test &#x22; (quotation mark)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x22;", out));
        EXPECT_EQ(out, "Start: \"");
    }

    // Test &#x27; (apostrophe)
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x27;", out));
        EXPECT_EQ(out, "Start: '");
    }

    // Test case insensitivity for 'x'
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#X26;", out));
        EXPECT_EQ(out, "Start: &");
    }
}

// Test that Unicode code points are correctly decoded
TEST(DecodeHtmlEntityTest, UnicodeCodePoints) {
    // Test Euro symbol (€) - 2 bytes in UTF-8
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#8364;", out));
        EXPECT_EQ(out, "Start: €");
    }

    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x20AC;", out));
        EXPECT_EQ(out, "Start: €");
    }

    // Test Copyright symbol (©) - 2 bytes in UTF-8
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#169;", out));
        EXPECT_EQ(out, "Start: ©");
    }

    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#xA9;", out));
        EXPECT_EQ(out, "Start: ©");
    }

    // Test Japanese character (日) - 3 bytes in UTF-8
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#26085;", out));
        EXPECT_EQ(out, "Start: 日");
    }

    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x65E5;", out));
        EXPECT_EQ(out, "Start: 日");
    }

    // Test Emoji (😀) - 4 bytes in UTF-8
    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#128512;", out));
        EXPECT_EQ(out, "Start: 😀");
    }

    {
        std::string out = "Start: ";
        EXPECT_TRUE(DecodeHtmlEntity("&#x1F600;", out));
        EXPECT_EQ(out, "Start: 😀");
    }
}

// Test that multiple entities can be decoded consecutively
TEST(DecodeHtmlEntityTest, MultipleEntitiesAppended) {
    std::string out = "Hello";

    EXPECT_TRUE(DecodeHtmlEntity("&lt;", out));
    EXPECT_EQ(out, "Hello<");

    EXPECT_TRUE(DecodeHtmlEntity("&gt;", out));
    EXPECT_EQ(out, "Hello<>");

    EXPECT_TRUE(DecodeHtmlEntity("&quot;", out));
    EXPECT_EQ(out, "Hello<>\"");
}

// Test that invalid entities are properly rejected
TEST(DecodeHtmlEntityTest, InvalidEntities) {
    // Test empty entity
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    // Test entities without semicolon
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&amp", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    // Test entities without ampersand
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("amp;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    // Test incomplete entities
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    // Test invalid numeric entities
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&#;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&#x;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&#invalid;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&#xinvalid;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    // Test out-of-range code points
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&#x110000;", out));  // Above valid Unicode range
        EXPECT_EQ(out, "Original");                         // out should be unchanged
    }

    // Test non-hex characters in hex entity
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&#xZ123;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }

    // Test unknown named entity
    {
        std::string out = "Original";
        EXPECT_FALSE(DecodeHtmlEntity("&unknown;", out));
        EXPECT_EQ(out, "Original");  // out should be unchanged
    }
}

// Test that strings with no entities remain unchanged
TEST(DecodeHtmlStringTest, NoEntities) {
    // Empty string
    EXPECT_EQ(DecodeHtmlString(""), "");

    // Simple text with no entities
    EXPECT_EQ(DecodeHtmlString("Hello, world!"), "Hello, world!");

    // Text with characters that look like entity parts but aren't
    EXPECT_EQ(DecodeHtmlString("Price & Quality"), "Price & Quality");
    EXPECT_EQ(DecodeHtmlString("Apples; Oranges"), "Apples; Oranges");
    EXPECT_EQ(DecodeHtmlString("Score: 10 < 20 > 5"), "Score: 10 < 20 > 5");
}

// Test that strings with a single entity are correctly decoded
TEST(DecodeHtmlStringTest, SingleEntity) {
    // Named entities
    EXPECT_EQ(DecodeHtmlString("Hello &amp; World"), "Hello & World");
    EXPECT_EQ(DecodeHtmlString("&lt;div&gt;"), "<div>");
    EXPECT_EQ(DecodeHtmlString("&quot;quoted&quot;"), "\"quoted\"");

    // Decimal entities
    EXPECT_EQ(DecodeHtmlString("Hello &#38; World"), "Hello & World");
    EXPECT_EQ(DecodeHtmlString("&#60;div&#62;"), "<div>");

    // Hex entities
    EXPECT_EQ(DecodeHtmlString("Hello &#x26; World"), "Hello & World");
    EXPECT_EQ(DecodeHtmlString("&#x3C;div&#x3E;"), "<div>");

    // Unicode entities
    EXPECT_EQ(DecodeHtmlString("Price: &#8364;100"), "Price: €100");
    EXPECT_EQ(DecodeHtmlString("&#x1F600; Smiling face"), "😀 Smiling face");
}

// Test that strings with multiple entities are correctly decoded
TEST(DecodeHtmlStringTest, MultipleEntities) {
    // Multiple named entities
    EXPECT_EQ(DecodeHtmlString("&lt;div&gt;Hello&lt;/div&gt;"), "<div>Hello</div>");
    EXPECT_EQ(DecodeHtmlString("&amp; &lt; &gt; &quot; &apos;"), "& < > \" '");

    // Multiple decimal entities
    EXPECT_EQ(DecodeHtmlString("&#38; &#60; &#62; &#34; &#39;"), "& < > \" '");

    // Multiple hex entities
    EXPECT_EQ(DecodeHtmlString("&#x26; &#x3C; &#x3E; &#x22; &#x27;"), "& < > \" '");

    // Mix of different entity types
    EXPECT_EQ(DecodeHtmlString("&lt;div&gt;&#38;&#x26;&amp;&lt;/div&gt;"), "<div>&&&</div>");

    // Adjacent entities
    EXPECT_EQ(DecodeHtmlString("&lt;&gt;&amp;"), "<>&");
}

// Test strings with a mix of entities and regular text
TEST(DecodeHtmlStringTest, MixedContent) {
    EXPECT_EQ(DecodeHtmlString("<p>This is a &quot;quoted&quot; text with special chars like &lt; &amp; &gt;</p>"),
              "<p>This is a \"quoted\" text with special chars like < & ></p>");

    EXPECT_EQ(DecodeHtmlString("HTML entities: &amp; for ampersand, &lt; for less than, &gt; for greater than"),
              "HTML entities: & for ampersand, < for less than, > for greater than");

    EXPECT_EQ(DecodeHtmlString("A mix of named (&amp;), decimal (&#38;), and hex (&#x26;) entities"),
              "A mix of named (&), decimal (&), and hex (&) entities");

    EXPECT_EQ(DecodeHtmlString("Unicode symbols: Euro &#8364;, Copyright &#169;, Degree &#176;"),
              "Unicode symbols: Euro €, Copyright ©, Degree °");
}

// Test handling of invalid or incomplete entities
TEST(DecodeHtmlStringTest, InvalidEntities) {
    // Entities without a closing semicolon
    EXPECT_EQ(DecodeHtmlString("This &amp is invalid"), "This &amp is invalid");
    EXPECT_EQ(DecodeHtmlString("This &lt is invalid"), "This &lt is invalid");

    // Entities without proper formatting
    EXPECT_EQ(DecodeHtmlString("This &invalid; entity"), "This &invalid; entity");
    EXPECT_EQ(DecodeHtmlString("This &#invalid; entity"), "This &#invalid; entity");
    EXPECT_EQ(DecodeHtmlString("This &#xinvalid; entity"), "This &#xinvalid; entity");

    // Lone ampersands
    EXPECT_EQ(DecodeHtmlString("This & that"), "This & that");
    EXPECT_EQ(DecodeHtmlString("A & B & C"), "A & B & C");

    // Mix of valid and invalid entities
    EXPECT_EQ(DecodeHtmlString("Valid &amp; and invalid &invalid;"), "Valid & and invalid &invalid;");
    EXPECT_EQ(DecodeHtmlString("Valid &#38; and invalid &#;"), "Valid & and invalid &#;");
}

// Test that entities at the start or end of a string are handled correctly
TEST(DecodeHtmlStringTest, PositionalEntities) {
    // Entity at the start
    EXPECT_EQ(DecodeHtmlString("&amp;start"), "&start");

    // Entity at the end
    EXPECT_EQ(DecodeHtmlString("end&amp;"), "end&");

    // Entity alone
    EXPECT_EQ(DecodeHtmlString("&amp;"), "&");

    // Multiple entities at start/end/alone
    EXPECT_EQ(DecodeHtmlString("&lt;&amp;&gt;"), "<&>");
}

// Test complex HTML snippets with multiple nested entities
TEST(DecodeHtmlStringTest, ComplexHtmlSnippets) {
    EXPECT_EQ(DecodeHtmlString(
                  "&lt;div class=&quot;container&quot;&gt;&lt;p&gt;Hello, &amp;nbsp;World!&lt;/p&gt;&lt;/div&gt;"),
              "<div class=\"container\"><p>Hello, &nbsp;World!</p></div>");

    EXPECT_EQ(DecodeHtmlString("Copyright &copy; 2023 &amp; Trademark &reg; &#8212; All rights reserved."),
              "Copyright © 2023 & Trademark ® — All rights reserved.");

    EXPECT_EQ(DecodeHtmlString("Special chars: &alpha; &beta; &gamma; &#8594; &#x2192;"), "Special chars: α β γ → →");
}

// Test very long strings with many entities
TEST(DecodeHtmlStringTest, LongStrings) {
    // Create a long string with repeated entities
    std::string longInput;
    std::string expectedOutput;

    for (int i = 0; i < 1000; i++) {
        longInput += "&lt;";
        expectedOutput += "<";
    }

    EXPECT_EQ(DecodeHtmlString(longInput), expectedOutput);

    // Long string with mixed content
    longInput = "";
    expectedOutput = "";

    for (int i = 0; i < 100; i++) {
        longInput += "Entity &amp; Text ";
        expectedOutput += "Entity & Text ";
    }

    EXPECT_EQ(DecodeHtmlString(longInput), expectedOutput);
}

// Test consecutive entities without spaces
TEST(DecodeHtmlStringTest, ConsecutiveEntities) {
    EXPECT_EQ(DecodeHtmlString("&lt;&gt;&amp;&quot;&apos;"), "<>&\"'");
    EXPECT_EQ(DecodeHtmlString("&#60;&#62;&#38;&#34;&#39;"), "<>&\"'");
    EXPECT_EQ(DecodeHtmlString("&#x3C;&#x3E;&#x26;&#x22;&#x27;"), "<>&\"'");
}
