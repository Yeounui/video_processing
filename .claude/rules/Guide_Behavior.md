---
description: Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.
---

# Core Principles

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" -> "Write tests for invalid inputs, then make them pass"
- "Fix the bug" -> "Write a test that reproduces it, then make it pass"
- "Refactor X" -> "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:

```text
1. [Step] -> verify: [check]
2. [Step] -> verify: [check]
3. [Step] -> verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

# Anti-Patterns and Snippets

Use the snippets below only when a principle needs concrete code examples. All explanatory content belongs in this guide. Snippet files should contain only brief comments and code.

| Principle | When to open | Anti-Pattern | Fix | Snippet |
|-----------|--------------|--------------|-----|---------|
| Think Before Coding | Hidden assumptions, ambiguous requests, premature implementation | Silently assumes file format, fields, scope | List assumptions explicitly, ask for clarification | `snippets/think_before_coding.py` |
| Simplicity First | Over-abstraction, speculative features, unnecessary configurability | Adds design patterns before requirements need them | Use the smallest implementation that solves today's request | `snippets/simplicity_first.py` |
| Surgical Changes | Drive-by refactors, style drift, unrelated edits | Reformats quotes, adds type hints, or rewrites adjacent code while fixing a bug | Only change lines that trace directly to the request | `snippets/surgical_changes.py` |
| Goal-Driven Execution | Vague tasks, missing verification, test-first bug fixes | "I'll review and improve the code" | Define a concrete reproduce -> fix -> verify loop | `snippets/goal_driven_execution.py` |

# Key Insight

The overcomplicated examples are not wrong because design patterns or best practices are bad. They are wrong because of timing: they add complexity before it is needed.

The simple versions are easier to understand, faster to implement, easier to test, and can be refactored later when complexity is actually needed.

**Good code is code that solves today's problem simply, not tomorrow's problem prematurely.**

When you need anti-pattern code examples or more precise instructions to comply with these principles, open the mapped snippet file for that principle.
