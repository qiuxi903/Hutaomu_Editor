// Hutaomu Editor (Rust) - 查找/替换引擎（对标 Qt 版 FindBar：大小写/全词/正则、$1..$9、上限）。
// SPDX-License-Identifier: AGPL-3.0-only

/// 匹配选项（对应 Qt 版三个复选框：区分大小写 / 全词 / 正则）
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct FindOptions {
    pub case_sensitive: bool,
    pub whole_word: bool,
    pub use_regex: bool,
}

impl Default for FindOptions {
    fn default() -> Self {
        // Qt 版默认不区分大小写
        FindOptions {
            case_sensitive: false,
            whole_word: false,
            use_regex: false,
        }
    }
}

/// 单个匹配的字节区间 [start, end)
#[derive(Clone, Copy, PartialEq, Debug)]
pub struct Match {
    pub start: usize,
    pub end: usize,
}

/// 匹配数量上限（防超大文档，与 Qt 版一致）
pub const MAX_MATCHES: usize = 2000;
/// 全部替换操作次数上限（与 Qt 版一致）
pub const MAX_REPLACE_OPS: usize = 100_000;

#[derive(Debug)]
pub enum FindError {
    InvalidRegex,
}

fn build_regex(pattern: &str, case_sensitive: bool) -> Result<regex::Regex, FindError> {
    regex::RegexBuilder::new(pattern)
        .case_insensitive(!case_sensitive)
        .size_limit(10 * 1024 * 1024)
        .build()
        .map_err(|_| FindError::InvalidRegex)
}

/// 找出全部匹配（字节偏移），最多 MAX_MATCHES 个
pub fn find_matches(text: &str, query: &str, opts: &FindOptions) -> Result<Vec<Match>, FindError> {
    if query.is_empty() {
        return Ok(Vec::new());
    }
    let mut out = Vec::new();
    if opts.use_regex {
        let pattern = if opts.whole_word {
            format!(r"\b(?:{})\b", query)
        } else {
            query.to_string()
        };
        let re = build_regex(&pattern, opts.case_sensitive)?;
        for m in re.find_iter(text) {
            out.push(Match {
                start: m.start(),
                end: m.end(),
            });
            if out.len() >= MAX_MATCHES {
                break;
            }
        }
    } else if opts.case_sensitive {
        for (start, _) in text.match_indices(query) {
            if !opts.whole_word || is_word_boundary(text, start, start + query.len()) {
                out.push(Match {
                    start,
                    end: start + query.len(),
                });
                if out.len() >= MAX_MATCHES {
                    break;
                }
            }
        }
    } else {
        // 不区分大小写：在折叠文本中找，再把折叠字节偏移映射回原文。
        // to_lowercase 可能改变字节长度（ß→ss），故映射需按字符对齐；
        // 匹配区间若不对齐到原字符边界则跳过。
        let folded = text.to_lowercase();
        let qf = query.to_lowercase();
        if qf.is_empty() {
            return Ok(Vec::new());
        }
        // (折叠文本中该字符起始字节, 原文中该字符起始字节)，fo 严格递增
        let mut map: Vec<(usize, usize)> = Vec::with_capacity(text.len());
        let mut orig_off = 0usize;
        let mut fold_off = 0usize;
        for ch in text.chars() {
            for lch in ch.to_lowercase() {
                map.push((fold_off, orig_off));
                fold_off += lch.len_utf8();
            }
            orig_off += ch.len_utf8();
        }
        for (fs, _) in folded.match_indices(&qf) {
            let idx_s = partition_point(&map, fs);
            if !map.get(idx_s).map(|(fo, _)| *fo == fs).unwrap_or(false) {
                continue;
            }
            let orig_start = map[idx_s].1;
            let fe = fs + qf.len();
            let orig_end = if fe >= folded.len() {
                text.len()
            } else {
                let idx_e = partition_point(&map, fe);
                if !map.get(idx_e).map(|(fo, _)| *fo == fe).unwrap_or(false) {
                    continue;
                }
                map[idx_e].1
            };
            if orig_end <= orig_start {
                continue;
            }
            if !opts.whole_word || is_word_boundary(text, orig_start, orig_end) {
                out.push(Match {
                    start: orig_start,
                    end: orig_end,
                });
                if out.len() >= MAX_MATCHES {
                    break;
                }
            }
        }
    }
    Ok(out)
}

/// map 中最后一个 fo <= target 的下标（fo 严格递增）
fn partition_point(map: &[(usize, usize)], target: usize) -> usize {
    let mut lo = 0usize;
    let mut hi = map.len();
    while lo < hi {
        let mid = (lo + hi) / 2;
        if map[mid].0 <= target {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    lo.saturating_sub(1)
}

fn is_word_boundary(text: &str, start: usize, end: usize) -> bool {
    let prev_alnum = text[..start]
        .chars()
        .next_back()
        .map(|c| c.is_alphanumeric() || c == '_')
        .unwrap_or(false);
    let next_alnum = text[end..]
        .chars()
        .next()
        .map(|c| c.is_alphanumeric() || c == '_')
        .unwrap_or(false);
    !prev_alnum && !next_alnum
}

/// 全部替换（支持正则 $1..$9 反向引用，regex 原生展开），返回 (新文本, 替换次数)
pub fn replace_all(
    text: &str,
    query: &str,
    replacement: &str,
    opts: &FindOptions,
) -> Result<(String, usize), FindError> {
    if query.is_empty() {
        return Ok((text.to_string(), 0));
    }
    if opts.use_regex {
        let pattern = if opts.whole_word {
            format!(r"\b(?:{})\b", query)
        } else {
            query.to_string()
        };
        let re = build_regex(&pattern, opts.case_sensitive)?;
        let count = re.find_iter(text).count().min(MAX_REPLACE_OPS);
        let new_text = re.replace_all(text, replacement).to_string();
        Ok((new_text, count))
    } else {
        let matches = find_matches(text, query, opts)?;
        let mut new_text = String::with_capacity(text.len());
        let mut last = 0usize;
        for m in &matches {
            new_text.push_str(&text[last..m.start]);
            new_text.push_str(replacement);
            last = m.end;
        }
        new_text.push_str(&text[last..]);
        Ok((new_text, matches.len()))
    }
}

/// 替换单个匹配并展开 $1..$9（"替换"按钮：先选区后替换的场景）
pub fn expand_one(
    text: &str,
    query: &str,
    m: Match,
    replacement: &str,
    opts: &FindOptions,
) -> String {
    if !opts.use_regex {
        return replacement.to_string();
    }
    let re = match build_regex(query, opts.case_sensitive) {
        Ok(re) => re,
        Err(_) => return replacement.to_string(),
    };
    match re.captures(&text[m.start..m.end]) {
        Some(caps) => {
            let mut out = String::new();
            caps.expand(replacement, &mut out);
            out
        }
        None => replacement.to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn find_case_insensitive() {
        let opts = FindOptions::default();
        let ms = find_matches("Hello hello HELLO", "hello", &opts).unwrap();
        assert_eq!(ms.len(), 3);
        assert_eq!(&"Hello hello HELLO"[ms[0].start..ms[0].end], "Hello");
    }

    #[test]
    fn find_case_sensitive() {
        let opts = FindOptions {
            case_sensitive: true,
            ..Default::default()
        };
        let ms = find_matches("Hello hello HELLO", "hello", &opts).unwrap();
        assert_eq!(ms.len(), 1);
    }

    #[test]
    fn find_whole_word() {
        let opts = FindOptions {
            whole_word: true,
            ..Default::default()
        };
        let ms = find_matches("cat catalog cat", "cat", &opts).unwrap();
        assert_eq!(ms.len(), 2);
        assert_eq!(ms[0].start, 0);
        assert_eq!(ms[1].start, 12);
    }

    #[test]
    fn find_regex() {
        let opts = FindOptions {
            use_regex: true,
            ..Default::default()
        };
        let ms = find_matches("a1b22c333", r"\d+", &opts).unwrap();
        assert_eq!(ms.len(), 3);
        assert_eq!(ms[1].start, 3);
        assert_eq!(ms[1].end, 5);
    }

    #[test]
    fn find_regex_invalid() {
        let opts = FindOptions {
            use_regex: true,
            ..Default::default()
        };
        assert!(matches!(
            find_matches("abc", "([", &opts),
            Err(FindError::InvalidRegex)
        ));
    }

    #[test]
    fn find_cjk_casefold() {
        let opts = FindOptions::default();
        let ms = find_matches("中文Test混合test", "test", &opts).unwrap();
        assert_eq!(ms.len(), 2);
    }

    #[test]
    fn find_sharp_s_fold() {
        // ß 折叠为 ss：不对齐原字符边界的匹配应跳过
        let opts = FindOptions::default();
        let ms = find_matches("Straße", "sse", &opts).unwrap();
        assert_eq!(ms.len(), 0);
        let ms2 = find_matches("Straße", "straße", &opts).unwrap();
        assert_eq!(ms2.len(), 1);
        assert_eq!(&"Straße"[ms2[0].start..ms2[0].end], "Straße");
    }

    #[test]
    fn replace_plain() {
        let opts = FindOptions::default();
        let (text, n) = replace_all("aa bb aa", "aa", "cc", &opts).unwrap();
        assert_eq!(text, "cc bb cc");
        assert_eq!(n, 2);
    }

    #[test]
    fn replace_regex_backref() {
        let opts = FindOptions {
            use_regex: true,
            ..Default::default()
        };
        let (text, n) =
            replace_all("John Smith, Jane Doe", r"(\w+) (\w+)", "$2 $1", &opts).unwrap();
        assert_eq!(text, "Smith John, Doe Jane");
        assert_eq!(n, 2);
    }

    #[test]
    fn replace_whole_word() {
        let opts = FindOptions {
            whole_word: true,
            ..Default::default()
        };
        let (text, n) = replace_all("cat catalog", "cat", "dog", &opts).unwrap();
        assert_eq!(text, "dog catalog");
        assert_eq!(n, 1);
    }

    #[test]
    fn max_matches_cap() {
        let opts = FindOptions::default();
        let text = "x".repeat(5000);
        let ms = find_matches(&text, "x", &opts).unwrap();
        assert_eq!(ms.len(), MAX_MATCHES);
    }
}
