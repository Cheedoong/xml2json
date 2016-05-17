xml2json
========

## A header-only C++ library converts XML to JSON

### Usage

xml2json is a header-only C++ library. Just copy the `./include` folder to system or project's include path. Two examples are shown in `xml2json.cpp` and `./test/test.cpp`

#### Build and Run a Command Line Tool
```bash
make
./xml2json input.xml
```
#### C++ API
```C++
std::string xml2json(const char *xml_str)
```

### Description

xml2json is the first carefully written C++ library that converts XML document to JSON format. It's already been used in the soft subtitle cross-domain solution at the server-end of Tencent Video (http://v.qq.com) and its CDNs. 

It's simple and fast, and with almost full support for the XML standards. 


It follows these simple but carefully considered rules below: 


<table>
<tbody><tr>
<td bgcolor="#CCCCCC"><strong>Pattern</strong>
</td>
<td bgcolor="#f0f0f0"><strong>XML</strong>
</td>
<td bgcolor="#CCCCCC"><strong>JSON</strong>
</td>
<td bgcolor="#f0f0f0"><strong>Access</strong>
</td>
</tr>

<tr>
<td bgcolor="#CCCCCC">1</td>
<td bgcolor="#f0f0f0">
<code>&lt;e/&gt;</code>
</td>
<td bgcolor="#CCCCCC">
<code>"e": null</code>
</td>
<td bgcolor="#f0f0f0">
<code>o.e</code>
</td>
</tr>

<tr>
<td bgcolor="#CCCCCC">2</td>
<td bgcolor="#f0f0f0">
<code>&lt;e&gt;text&lt;/e&gt;</code>
</td>
<td bgcolor="#CCCCCC">
<code>"e": "text"</code>
</td>
<td bgcolor="#f0f0f0">
<code>o.e</code>
</td>
</tr>

<tr>
<td bgcolor="#CCCCCC">3</td>
<td bgcolor="#f0f0f0">
<code>&lt;e name="value" /&gt;</code>
</td>
<td bgcolor="#CCCCCC">
<code>"e":{"@name": "value"}</code>
</td>
<td bgcolor="#f0f0f0">
<code>o.e["@name"]</code>
</td>
</tr>

<tr>
<td bgcolor="#CCCCCC">4</td>
<td bgcolor="#f0f0f0">
<code>&lt;e name="value"&gt;text&lt;/e&gt;</code>
</td>
<td bgcolor="#CCCCCC">
<code>"e": {
  "@name": "value",
  "#text": "text"
}</code>
</td>
<td bgcolor="#f0f0f0">
<code>o.e["@name"]
o.e["#text"]</code>
</td>
</tr>

<tr>
<td bgcolor="#CCCCCC">5</td>
<td bgcolor="#f0f0f0">
<code>&lt;e&gt;
  &lt;a&gt;text&lt;/a&gt;
  &lt;b&gt;text&lt;/b&gt;
&lt;/e&gt;</code>
</td>
<td bgcolor="#CCCCCC">
<code>"e": { 
  "a": "text",
  "b": "text"
}</code>
</td>
<td bgcolor="#f0f0f0">
<code>o.e.a
o.e.b</code>
</td>
</tr>

<tr>
<td bgcolor="#CCCCCC">6</td>
<td bgcolor="#f0f0f0">
<code>&lt;e&gt;
  &lt;a&gt;text&lt;/a&gt;
  &lt;a&gt;text&lt;/a&gt;
&lt;/e&gt;</code>
</td>
<td bgcolor="#CCCCCC">
<code>"e": {
  "a": ["text", "text"]
}</code>
</td>
<td bgcolor="#f0f0f0">
<code>o.e.a[0]
o.e.a[1]</code>
</td>
</tr>

<tr>
<td bgcolor="#CCCCCC">7</td>
<td bgcolor="#f0f0f0">
<code>&lt;e&gt;
  text
  &lt;a&gt;text&lt;/a&gt;
&lt;/e&gt;</code>
</td>
<td bgcolor="#CCCCCC">
<code>"e": {
  "#text": "text",
  "a": "text"
}</code>
</td>
<td bgcolor="#f0f0f0">
<code>o.e["#text"]
o.e.a</code>
</td>
</tr>
</tbody></table>

### Tested Under
+ Linux
+ Mac OS X
+ Windows
+ OpenWRT

### Donate
![WeChat Pay](https://raw.githubusercontent.com/Cheedoong/xml2json/master/donate/wechatpay_cheedoong.png)  
![AliPay](https://raw.githubusercontent.com/Cheedoong/xml2json/master/donate/alipay_cheedoong.jpg)
