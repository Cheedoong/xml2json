xml2json
========

a C++ header-only library to convert XML document to JSON format


the first carefully written C++ library that converts XML to JSON, already used in the soft subtitle cross-domain solution at the server-end of Tencent Video (http://v.qq.com) and its CDNs

simple and fast, and almost fully support for XML standards


it follows these simple but carefully considered rules below: 


	Pattern	XML							JSON										Access
1	<e/>								"e": null									o.e
2	<e>text</e>							"e": "text"									o.e
3	<e name="value" />					"e": {"@name": "value"}						o.e["@name"]
4	<e name="value">text</e>			"e": { "@name": "value", "#text": "text" }	o.e["@name"] o.e["#text"]
5	<e> <a>text</a> <b>text</b> </e>	"e": { "a": "text", "b": "text" }			o.e.a o.e.b
6	<e> <a>text</a> <a>text</a> </e>	"e": { "a": ["text", "text"] }				o.e.a[0] o.e.a[1]
7	<e> text <a>text</a> </e>			"e": { "#text": "text", "a": "text" }		o.e["#text"] o.e.a

