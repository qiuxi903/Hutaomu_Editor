package tree_sitter_php_test

import (
	"strings"
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_php "github.com/tree-sitter/tree-sitter-php/bindings/go"
)

func TestPHPGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_php.LanguagePHP())
	if language == nil {
		t.Errorf("Error loading PHP grammar")
	}

	sourceCode := []byte("<?php echo 'Hello, World!';")
	parser := tree_sitter.NewParser()
	defer parser.Close()
	parser.SetLanguage(language)

	tree := parser.Parse(sourceCode, nil)
	if tree == nil || tree.RootNode().HasError() {
		t.Errorf("Error parsing PHP")
	}
}

func TestPHPOnlyGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_php.LanguagePHPOnly())
	if language == nil {
		t.Errorf("Error loading PHP-Only grammar")
	}

	sourceCode := []byte("echo 'Hello, World!';")
	parser := tree_sitter.NewParser()
	defer parser.Close()
	parser.SetLanguage(language)

	tree := parser.Parse(sourceCode, nil)
	if tree == nil || tree.RootNode().HasError() {
		t.Errorf("Error parsing PHP")
	}
}

func sequentialHeredocs(prefix string, count int, tag string) []byte {
	block := "$a = <<<" + tag + "\nx\n" + tag + ";\n"
	src := make([]byte, 0, len(prefix)+len(block)*count)
	src = append(src, prefix...)
	for i := 0; i < count; i++ {
		src = append(src, block...)
	}
	return src
}

func parseWithoutError(t *testing.T, language *tree_sitter.Language, source []byte) {
	t.Helper()
	parser := tree_sitter.NewParser()
	defer parser.Close()
	parser.SetLanguage(language)
	tree := parser.Parse(source, nil)
	if tree == nil || tree.RootNode().HasError() {
		t.Errorf("Error parsing sequential heredocs")
	}
}

func TestManySequentialHeredocs(t *testing.T) {
	php := tree_sitter.NewLanguage(tree_sitter_php.LanguagePHP())
	phpOnly := tree_sitter.NewLanguage(tree_sitter_php.LanguagePHPOnly())
	longTag := strings.Repeat("HEREDOC_", 30)
	parseWithoutError(t, php, sequentialHeredocs("<?php\n", 20, longTag))
	parseWithoutError(t, php, sequentialHeredocs("<?php\n", 250, "EOD"))
	parseWithoutError(t, phpOnly, sequentialHeredocs("", 20, longTag))
	parseWithoutError(t, phpOnly, sequentialHeredocs("", 250, "EOD"))
}
