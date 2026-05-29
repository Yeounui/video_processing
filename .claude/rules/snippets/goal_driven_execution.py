# Goal-Driven Execution: bad example - large implementation without verification steps.
# Implements full rate limiting with Redis, multiple strategies,
# configuration system, and monitoring in one 300-line commit
# No clear verification steps


# Goal-Driven Execution: bad example - fixes without reproducing the reported bug.
def sort_scores(scores):
    return sorted(scores, key=lambda x: (-x['score'], x['name']))


# Goal-Driven Execution: good example - reproduce first, then fix.
def test_sort_with_duplicate_scores():
    """Test sorting when multiple items have same score."""
    scores = [
        {'name': 'Alice', 'score': 100},
        {'name': 'Bob', 'score': 100},
        {'name': 'Charlie', 'score': 90},
    ]

    result = sort_scores(scores)

    # The bug: order is non-deterministic for duplicates
    # Run this test multiple times, it should be consistent
    assert result[0]['score'] == 100
    assert result[1]['score'] == 100
    assert result[2]['score'] == 90

# Verify: Run test 10 times -> fails with inconsistent ordering

# 2. Now fix with stable sort
def sort_scores(scores):
    """Sort by score descending, then name ascending for ties."""
    return sorted(scores, key=lambda x: (-x['score'], x['name']))

# Verify: Test passes consistently
