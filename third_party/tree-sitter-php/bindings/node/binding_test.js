/// <reference types="node" />

const assert = require("node:assert");
const { describe, it } = require("node:test");

const Parser = require("tree-sitter");
const { php, php_only } = require("../..");

describe("PHP", () => {
  const parser = new Parser();
  parser.setLanguage(php);

  it("should be named php", () => {
    assert.strictEqual(parser.getLanguage().name, "php");
  });

  it("should parse source code", () => {
    const sourceCode = "<?php echo 'Hello, World!';";
    const tree = parser.parse(sourceCode);
    assert(!tree.rootNode.hasError);
  });

  it("should parse many sequential heredocs", () => {
    const longTag = "HEREDOC_".repeat(30);
    const long = "<?php\n" + `$a = <<<${longTag}\nx\n${longTag};\n`.repeat(20);
    const short = "<?php\n" + "$a = <<<EOD\nx\nEOD;\n".repeat(250);
    assert(!parser.parse(long).rootNode.hasError);
    assert(!parser.parse(short).rootNode.hasError);
  });
});

describe("PHP Only", () => {
  const parser = new Parser();
  parser.setLanguage(php_only);

  it("should be named php_only", () => {
    assert.strictEqual(parser.getLanguage().name, "php_only");
  });

  it("should parse source code", () => {
    const sourceCode = "echo 'Hello, World!';";
    const tree = parser.parse(sourceCode);
    assert(!tree.rootNode.hasError);
  });

  it("should parse many sequential heredocs", () => {
    const longTag = "HEREDOC_".repeat(30);
    const long = `$a = <<<${longTag}\nx\n${longTag};\n`.repeat(20);
    const short = "$a = <<<EOD\nx\nEOD;\n".repeat(250);
    assert(!parser.parse(long).rootNode.hasError);
    assert(!parser.parse(short).rootNode.hasError);
  });
});
