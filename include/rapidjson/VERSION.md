# Vendored rapidjson

Last reviewed: 2026-10-27

## Baseline

| Item | Value |
|---|---|
| Version | 1.1.0 plus the post-1.1.0 fixes on upstream `master` |
| Upstream | [`Tencent/rapidjson`](https://github.com/Tencent/rapidjson) |
| Baseline commit | `24b5e7a8b27f42fa16b96fc70aade9106cf7102f` |
| Baseline date | 2024-12-22 |
| Baseline commit message | `Fix out of bounds read with kParseValidateEncodingFlag` |
| Verified | `rapidjson.h`, `document.h`, `reader.h` and `writer.h` are blob-identical to upstream `master` (2026-10-27) |

Upstream has not tagged a release since `v1.1.0` (2016-08-25), so `master` is
the newest available code.  Pin the commit above; do **not** use the `v1.1.0`
tag as the dependency version.

## CVE status

| CVE | Status |
|---|---|
| `CVE-2024-38517` | Covered by upstream commit `8269bc2` (2018-05-16, `Prevent int underflow when parsing exponents`). `8269bc2` is an ancestor of the baseline (`ahead_by: 405`). |
| `CVE-2024-39684` | Same `GenericReader::ParseNumber()` area. No separate fix commit was found. Upstream issues [#2289](https://github.com/Tencent/rapidjson/issues/2289) and [#2303](https://github.com/Tencent/rapidjson/issues/2303) are still open; the report is disputed and was made against `1.1.0`. |

Do not describe this as two independent CVE patches. The 2018 `ParseNumber`
fix is the relevant code-level fix; `CVE-2024-39684` needs the caveat above.

If the negative tests later reproduce a crash, replace the `CVE-2024-39684`
row with:

```text
CVE-2024-39684: NOT COVERED; see upstream issues #2289/#2303;
json2xml input treated as untrusted.
```

## Downstream patches

None currently applied.

Candidate, not applied:

- `thorsten-klein/rapidjson` commit `824a72ac` — `Handle allocation failure
  instead of writing through NULL`.
- This is gated by the R2 probe and has not been merged into this tree.
- If applied, record the patch SHA, the three affected headers, the
  hunk/API matrix, and the rollback steps here.

## Scope

- Primary protection of the R2 candidate: the `json2xml` RapidJSON parse path.
- `xml2json` still parses XML with RapidXML.  The `document.h` build path only
  gets partial hardening; the build-layer harness is not an end-to-end
  `xml2json()` verification.
- Do not claim that `xml2json` is fully hardened because of R2.

## Reference forks

- `AccuWeather-Inc/rapidjson` `v2.0.5` (`66b8e365`): versioning/CHANGELOG
  reference only.  It is not used as this project's baseline.
- `Esri/rapidjson` `release/200.8`: not used; the branch is based on a 2018
  upstream commit and is far behind the baseline.

## Verification

```bash
# CVE fix is an ancestor of the vendored baseline
git merge-base --is-ancestor 8269bc2 24b5e7a8

# Core files are byte-identical to the pinned upstream commit
# (run in a temporary upstream clone, or compare hashes)
```

Project-level verification: CTest, the 55+ regression checks, C++11/14/17/20
with `-Wall -Wextra -Werror`, golden output sizes 690 / 77562 / 91260 bytes,
and the differential suite against `master`.

## Review triggers

Re-review this file when any of the following happens:

- upstream `master` moves past `24b5e7a8`;
- `824a72ac` is merged upstream;
- RapidJSON publishes a new tag/release;
- a new CVE or security advisory appears;
- the `xml2json` / `json2xml` API or allocator usage changes;
- the compiler/toolchain changes in a way that affects OOM behavior;
- the downstream patch no longer applies cleanly;
- the OOM regression fails in CI;
- quarterly review.
