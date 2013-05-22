#include <Rinternals.h>
#include <string.h>

static SEXP mat_split_mem(const char *mem, size_t len, char sep, unsigned long line) {
    unsigned int ncol = 1, nrow, np = 0, i, N;
    unsigned long lines = 1;
    SEXP res;
    const char *e = memchr(mem, '\n', len), *ee = mem + len, *c, *l;
    if (!e) e = ee; /* e is the end of the first line - needed to count columns */
    c = mem;
    /* first pass - find the # of lines */
    while ((c = memchr(c, '\n', ee - c))) { c++; lines++; }
    if (ee > mem && ee[-1] == '\n') lines--; /* don't count the last empty one */
    nrow = lines;
    c = mem;
    while ((c = memchr(c, (unsigned char) sep, e - c))) { ncol++; c++; }
    N = ncol * nrow;
    res = PROTECT(allocMatrix(STRSXP, nrow, ncol));
    np++;
    l = mem;
    for (i = 0; i < nrow; i++) {
	const char *le = memchr(l, '\n', ee - l);
	int j = i;
	if (!le) le = ee;
	while ((c = memchr(l, (unsigned char) sep, le - l))) {
	    if (j >= N)
                Rf_error("line %lu: too many columns (expected %d)", line + (unsigned long)(i + 1), ncol);
	    SET_STRING_ELT(res, j, Rf_mkCharLen(l, c - l));
	    l = c + 1;
            j += nrow;
        }
	/* add last entry */
        if (j >= N)
            Rf_error("line %lu: too many columns (expected %d)", l + (unsigned long)(i + 1), ncol);
	SET_STRING_ELT(res, j, Rf_mkCharLen(l, le - l));
        /* fill up with NAs */
        j += nrow;
        while (j < N) {
            SET_STRING_ELT(res, j, R_NaString);
            j += nrow;
        }
	l = le + 1;
	if (l >= ee) break; /* should never be necesary */
    }
    UNPROTECT(1);
    return res;
}

SEXP mat_split(SEXP s, SEXP sSep, SEXP sLine) {
    unsigned long line = 1;
    unsigned int ncol = 1, nrow, np = 0, i, N;
    SEXP res;
    char sep;
    const char *c;
    if (TYPEOF(sLine) == REALSXP && LENGTH(sLine))
	line = (unsigned long) asReal(sLine);
    else if (TYPEOF(sLine) == INTSXP && LENGTH(sLine))
	line = (unsigned long) sLine;
    if (TYPEOF(sSep) != STRSXP || LENGTH(sSep) < 1)
	Rf_error("invalid separator");
    sep = CHAR(STRING_ELT(sSep, 0))[0];
    if (TYPEOF(s) != STRSXP) {
	if (TYPEOF(s) == RAWSXP)
	    return mat_split_mem(RAW(s), LENGTH(s), sep, line);
	s = PROTECT(coerceVector(s, STRSXP));
	np++;
    }
    nrow = LENGTH(s);
    if (!nrow) {
	if (np) UNPROTECT(np);
	return allocMatrix(STRSXP, 0, 0);
    }
    /* count the separators in the first line (ncol=1 on init) */
    c = CHAR(STRING_ELT(s, 0));
    while ((c = strchr(c, sep))) { ncol++; c++; }
    N = ncol * nrow;
    res = PROTECT(allocMatrix(STRSXP, nrow, ncol));
    np++;
    for (i = 0; i < nrow; i++) {
	const char *l = CHAR(STRING_ELT(s, i));
	int j = i;
	while ((c = strchr(l, sep))) {
	    if (j >= N)
		Rf_error("line %lu: too many columns (expected %d)", line + (unsigned long)(i + 1), ncol);
	    SET_STRING_ELT(res, j, Rf_mkCharLen(l, c - l));
	    l = c + 1;
	    j += nrow;
	}
	/* add last entry */
	if (j >= N)
	    Rf_error("line %lu: too many columns (expected %d)", l + (unsigned long)(i + 1), ncol);
	SET_STRING_ELT(res, j, Rf_mkChar(l));
	/* fill up with NAs */
	j += nrow;
	while (j < N) {
	    SET_STRING_ELT(res, j, R_NaString);
	    j += nrow;
	}
    }
    UNPROTECT(np);
    return res;
}