#!/usr/bin/env python3

import json
import requests

inFileName = "../../README.md"
outFileName = "../../docs/index.html"

htmlBeforeBody = '''
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" lang="" xml:lang="">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>NVTX - NVIDIA Tools Extension Library</title>
  <link rel="stylesheet" href="github-markdown.css">
  <style>
    .markdown-body {
      box-sizing: border-box;
      min-width: 200px;
      max-width: 980px;
      margin: 0 auto;
      padding: 45px;
    }
  
    @media (max-width: 767px) {
      .markdown-body {
        padding: 15px;
      }
    }
  </style>
</head>
<body class="markdown-body" style="background-color: var(--color-canvas-default)">
'''

htmlAfterBody = '''
</body>
</html>
'''

try:
    with open(inFileName, "r+") as inFile:
        markdownText = inFile.read()
except IOError:
    print("Failed to open input file ", inFileName)
    sys.exit(1)

# Replace relative intra-repo links with full URLs, assuming release-v3 branch
repoBaseUrl = "https://github.com/NVIDIA/NVTX/tree/release-v3"
markdownText = markdownText.replace("(/c)",      "(" + repoBaseUrl + "/c)")
markdownText = markdownText.replace("(/python)", "(" + repoBaseUrl + "/python)")

# Github replaces image links to external sites in README files with mangled things
# for "security".  This means README.md cannot directly link to docs/images using
# github.io links (which it treats as external) with getting them mangled.  Solution
# is to instead have READMEs on github link to raw.githubusercontent.com.  Replace
# those links here with local links, so README.md converted to index.html for
# github.io will get the local images from github.io's hosting.
rawContextBaseUrl = "https://raw.githubusercontent.com/NVIDIA/NVTX/release-v3/docs/"
markdownText = markdownText.replace(rawContextBaseUrl, "")

# Replace mentions of "this repo", which makes sense in the GitHub repo's markdown files,
# with a name that makes more sense in docs hosted outside the GitHub repo.
markdownText = markdownText.replace("this repo", "the NVIDIA NVTX GitHub repo")

url = "https://api.github.com/markdown"

postData = {"text": markdownText, "mode": "markdown"}

try:
    response = requests.post(url, json = postData)
    response.raise_for_status()
except Exception as ex:
    print("Failure in API call to GitHub to convert markdown to html:", ex)
    sys.exit(1)

html = htmlBeforeBody + response.text + htmlAfterBody

try:
    with open(outFileName, "w") as outFile:
        outFile.write(html)
except IOError:
    print("Failed to open output file ", outFileName)
    sys.exit(1)

print(f'Successfully generated "{outFileName}" from "{inFileName}".')