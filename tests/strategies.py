from hypothesis import strategies as st


safe_text = st.text(
    min_size=1, max_size=2048, alphabet=st.characters(blacklist_categories=("Cs"))
)
finite_unit = st.floats(
    min_value=0.0, max_value=1.0, allow_nan=False, allow_infinity=False
)
invalid_unit = st.one_of(
    st.floats(max_value=-1e-9, allow_nan=False, allow_infinity=False),
    st.floats(min_value=1.0 + 1e-9, allow_nan=False, allow_infinity=False),
    st.just(float("nan")),
    st.just(float("inf")),
    st.just(float("-inf")),
)
