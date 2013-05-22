#include <stdlib.h>
#include <string.h>

#include <Rinternals.h>
#include <R_ext/Connections.h>

#if R_CONNECTIONS_VERSION != 1
#error "Missing or unsupported connection API in R"
#endif

/* this should really be in R_ext/Connections.h */
extern Rconnection getConnection(int n);

typedef struct chunk_read {
    int len, alloc;
    SEXP sConn;
    Rconnection con;
    char buf[1];
} chunk_read_t;

static void chunk_fin(SEXP ref) {
    chunk_read_t *r = (chunk_read_t*) R_ExternalPtrAddr(ref);
    if (r) {
	if (r->sConn) R_ReleaseObject(r->sConn);
	free(r);
    }
}

static const char *reader_class = "ChunkReader";

SEXP create_chunk_reader(SEXP sConn, SEXP sMaxLine) {
    int max_line = asInteger(sMaxLine);
    Rconnection con;
    chunk_read_t *r;
    SEXP res;

    if (!inherits(sConn, "connection"))
	Rf_error("invalid connection");
    if (max_line < 64) Rf_error("invalid max.line (must be at least 64)");
    
    con = getConnection(asInteger(sConn));
    r = (chunk_read_t*) malloc(sizeof(chunk_read_t) + max_line);
    if (!r) Rf_error("Unable to allocate %.3Mb for line buffer", ((double) max_line) / (1024.0*1024.0));
    r->len   = 0;
    r->sConn = sConn;
    r->con   = con;
    r->alloc = max_line;

    res = PROTECT(R_MakeExternalPtr(r, R_NilValue, R_NilValue));
    R_RegisterCFinalizerEx(res, chunk_fin, TRUE);
    R_PreserveObject(sConn);
    setAttrib(res, R_ClassSymbol, mkString(reader_class));
    UNPROTECT(1);
    return res;
}

SEXP chunk_read(SEXP sReader, SEXP sMaxSize) {
    SEXP res;
    int max_size = asInteger(sMaxSize), i, n;
    chunk_read_t *r;
    char *c;

    if (!inherits(sReader, reader_class))
	Rf_error("invalid reader");
    r = (chunk_read_t*) R_ExternalPtrAddr(sReader);
    if (!r) Rf_error("invalid NULL reader");
    if (max_size < r->alloc) Rf_error("invalid max.size - must be more than the max. line (%d)", r->alloc);

    res = allocVector(RAWSXP, max_size);
    c = (char*) RAW(res);
    if ((i = r->len)) {
	memcpy(c, r->buf, r->len);
	r->len = 0;
    }
    while (i < max_size) {
	n = R_ReadConnection(r->con, c + i, max_size - i);
	if (n < 1) { /* nothing to read, return all we got so far */
	    SEXP tmp = PROTECT(res);
	    res = allocVector(RAWSXP, i);
	    if (LENGTH(res)) memcpy(RAW(res), RAW(tmp), i);
	    UNPROTECT(1);	    
	    return res;
	}
	i += n;
	n = i;
	/* find the last newline */
	while (--i >= 0)
	    if (c[i] == '\n') break;
	if (i >= 0) { /* found a newline */
	    if (n - i > 1) { /* trailing content (incomplete next line) */
		if (n - i - 1 > r->alloc)
		    Rf_error("line too long (%d, max.line was set to %d for this reader), aborting", n - i - 1, r->alloc);
		r->len = n - i - 1;
		memcpy(r->buf, c + i + 1, r->len);
	    }
	    SETLENGTH(res, i + 1); /* include the newline */
	    return res;
	}
	/* newline not found, try to read some more */
	i = n;
    }
    Rf_error("line too long, it exceeds even max.size");
    /* unreachable */
    return R_NilValue;
}