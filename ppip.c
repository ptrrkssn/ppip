/*
** ppip.c - Unix clone of CP/M PIP tool
**
** Author: Peter Eriksson <pen@lysator.liu.se>
** Date:   2015-05-12
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

char *argv0 = "ppip";
extern char version[];

int verbose = 0;

char *delim_dst = "_=";
char *delim_src = ", ";

typedef struct {
    int ccase;
    int lineno;
    int detabify;
    int zeroparity;
    int filter_ff;
    int insert_ff;
    int echo;
    int max_col;
    int binary;
} Flags;

Flags gf;


char *
concat3(const char *s1,
	const char *s2,
	const char *s3)
{
    char *dst = malloc(strlen(s1)+strlen(s2)+strlen(s3)+1);
    if (!dst)
	return NULL;

    strcpy(dst, s1);
    strcat(dst, s2);
    strcat(dst, s3);
    
    return dst;
}


int
flags_parse(Flags *fp,
	    char *opt)
{
    int n = 0, o, v;

    while (*opt && *opt != ']')
    {
	o = *opt++;
	if (isdigit(*opt))
	{
	    v = 0;
	    while (isdigit(*opt))
		v = v*10+(*opt++ - '0');
	}
	else
	    v = 1;
	
	switch (o)
	{
	  case 'O':
	    fp->binary = v;
	    break;
	    
	  case 'N':
	    fp->lineno = v;
	    break;
	    
	  case 'E':
	    fp->echo = v;
	    break;
	    
	  case 'T':
	    fp->detabify = (v == 1 ? 8 : v);
	    break;
	    
	  case 'D':
	    fp->max_col = (v == 1 ? 80 : v);
	    break;
	    
	  case 'U':
	    fp->ccase = 'U';
	    break;
	    
	  case 'L':
	    fp->ccase = 'L';
	    break;

	  case 'Z':
	    fp->zeroparity = v;
	    break;

	  case 'F':
	    fp->filter_ff = v;
	    break;
	    
	  case 'P':
	    fp->insert_ff = (v == 1 ? 60 : v);
	    break;
	    
	  default:
	    return -1;
	}
	++n;
    }

    return n;
}

int
is_dir(const char *path)
{
    struct stat sb;

    if (stat(path, &sb) < 0)
	return -1;

    return S_ISDIR(sb.st_mode);
}




void
error(int ecode,
      const char *fmt,
      ...)
{
    va_list ap;


    va_start(ap, fmt);
    fprintf(stderr, "%s: %s: ", argv0, ecode == 0 ? "Warning" : "Error");
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);

    if (ecode)
	exit(ecode);
}


int
pip(char *arg)
{
    int c, lc = -1, lno, col;
    char *dst, *src, *opt;
    FILE *d_fp = NULL, *s_fp = NULL;
    unsigned long long b;
    int dst_dir_f = 0;
    Flags df;


    df = gf;
    
    lno = col = b = 0;
    
    dst = strtok(arg, delim_dst);
    if ((opt = strchr(dst, '[')) != NULL)
    {
	*opt++ = '\0';
	flags_parse(&df, opt);
    }
    
    dst_dir_f = (is_dir(dst) > 0);
    
    if (!dst_dir_f)
    {
	d_fp = fopen(dst, "w");
	if (!d_fp)
	    error(1, "%s: fopen: %s", dst, strerror(errno));
	
	if (verbose)
	    fprintf(stderr, "%s:\n", dst);
    }
    
    
    while ((src = strtok(NULL, delim_src)) != NULL)
    {
	char *dst_fname = NULL;
	Flags lf;
	lf = df;
	
	opt = strchr(src, '[');
	if (opt)
	{
	    *opt++ = '\0';
	    flags_parse(&lf, opt);
	}
	
	if (dst_dir_f)
	{
	    char *t = strrchr(src, '/');
	    
	    if (t)
		dst_fname = concat3(dst, "/", t+1);
	    else
		dst_fname = concat3(dst, "/", src);
	    
	    d_fp = fopen(dst_fname, "w");
	    if (!d_fp)
		error(1, "%s: fopen: %s", dst_fname, strerror);
	    
	    lno = col = b = 0;
	    
	    if (verbose)
		fprintf(stderr, "%s:\n", dst_fname);
	}
	
	s_fp = fopen(src, "r");
	if (!s_fp)
	    error(1, "%s: fopen: %s", src, strerror(errno));
	
	while ((c = getc(s_fp)) != EOF)
	{
	    ++b;
	    
	    if (!lf.binary)
	    {
		if (lf.filter_ff && c == '\f')
		    continue;
		
		if (lf.lineno && (lc == -1 || lc == '\n' || lc == '\f'))
		{
		    if (lf.lineno > 1)
			fprintf(d_fp, "%05u\t", lno+1); /* XXX: Fix detabify */
		    else
			fprintf(d_fp, "%5u:", lno+1);
		}
		
		switch (lf.ccase)
		{
		  case 'U':
		    c = toupper(c);
		    break;
		  case 'L':
		    c = tolower(c);
		    break;
		}
	    }
	    
	    if (lf.zeroparity)
		c &= 0x7f;
	    
	    lc = c;
	    
	    if (!lf.binary)
	    {
		if (lf.detabify && c == '\t')
		{
		    do
		    {
			if (lf.max_col && col >= lf.max_col)
			    break;
			
			putc(' ', d_fp);
			if (lf.echo)
			    putchar(' ');
		    } while ((++col % lf.detabify) != 0);
		}
		else
		{
		    if (c == '\n' || c == '\f')
		    {
			if (lf.insert_ff && ((lno+1) % lf.insert_ff) == 0 && c != '\f')
			{
			    putc('\f', d_fp);
			    if (lf.echo)
				putchar('\f');
			}
			
			putc(c, d_fp);
			if (lf.echo)
			    putchar(c);
			
			if (c == '\n')
			{
			    col = 0;
			    ++lno;
			}
		    }
		    else if (!lf.max_col || col < lf.max_col)
		    {
			putc(c, d_fp);
			if (lf.echo)
			    putchar(c);
			
			++col;
		    }
		}
	    }
	    else
	    {
		putc(c, d_fp);
		if (lf.echo)
		    putchar(c);
	    }
	}
	
	fclose(s_fp);
	
	if (dst_dir_f)
	{
	    free(dst_fname);
	    fclose(d_fp);
	}
	
	if (verbose)
	{
	    if (lf.binary)
		fprintf(stderr, "  %s: Copied %llu bytes\n", src, b);
	    else
		fprintf(stderr, "  %s: Copied %llu characters (%u lines)\n", src, b, lno);
	}
    }
    
    return fclose(d_fp);
}


int
main(int argc,
     char *argv[])
{
    int i, j;
    

    argv0 = argv[0];
    memset(&gf, 0, sizeof(gf));
    
    for (i = 1; i < argc && argv[i][0] == '-'; i++)
    {
	for (j = 1; argv[i][j]; j++)
	{
	    switch (argv[i][j])
	    {
	      case 'h':
		printf("[PPIP version %s - Copyright (c) 2014 Peter Eriksson <pen@lysator.liu.se>]\n", version);
		puts("\nUsage:");
		printf("    %s [<switches>] [<dst>=<src>[,<src>]*]*\n", argv[0]);
		puts("\nSwitches:");
		puts("    -h        Display this information");
		puts("    -v        Be verbose");
		puts("    -d<chrs>  Destination delimiter (default: =)");
		puts("    -s<chrs>  Source delimiter (default: ,)");
		puts("    -o<opts>  Global copying options");
		puts("\nSource & Destination:");
		puts("    <dst> and <src> may optionally include copying options inside []");
		puts("\nOptions:");
		puts("    E         Echo everything to standard output");
		puts("    O         Binary mode copy");
		puts("    U         Convert to upper case");
		puts("    L         Convert to lower case");
		puts("    F         Filter form feed characters from input");
		puts("    Z         Zero the parity bit on input for each character");
		puts("    D[<n>]    Truncate characters that extend after column <n> (default: 80)");
		puts("    T[<n>]    Expand TAB to SPC. Tab stops every 'n' character (default: 8)");
		puts("    N[<n>]    Add line number+colon or if 'n' > 1, zeropad+tab");
		puts("    P[<n>]    Add form feed every <n> line (default: 60)");
		puts("\nExample:");
		printf("    %s -oN2 'to1.txt[U]=from1.txt[T],from2.txt' to2.txt=from3.txt\n", argv[0]);
		puts("\nDescription:");
		puts("    Welcome to Peter's CP/M PIP clone/variant. This tool tries to mimic");
		puts("    the classic Peripheral Interchange Program, with some adaptions for");
		puts("    Unix systems and was written by Peter Eriksson <pen@lysator.liu.se>.");
		exit(0);

	      case 'v':
		++verbose;
		break;

	      case 'd':
		if (argv[i][j+1])
		    delim_dst = strdup(argv[i]+j+1);
		else
		    delim_dst = "=";
		goto NextArg;

	      case 's':
		if (argv[i][j+1])
		    delim_src = strdup(argv[i]+j+1);
		else
		    delim_src = ",";
		goto NextArg;
		
	      case 'o':
		flags_parse(&gf, argv[i]+j+1);
		goto NextArg;
		
	      default:
		fprintf(stderr, "%s: Invalid switch: -%c\n",
			argv[0], argv[i][j]);
		exit(1);
	    }
	}
      NextArg:;
    }

    if (i <argc)
    {
	for (; i < argc; i++)
	    pip(argv[i]);
    }
    else
    {
	char buf[1024], *cp;


	while ((isatty(0) ? fputs("> ", stderr) == 2 : 1) &&
	       fgets(buf, sizeof(buf), stdin) != NULL)
	{
	    for (cp = buf+strlen(buf); cp > buf && isspace(cp[-1]); --cp)
		;
	    *cp = '\0';
	    for (cp = buf; *cp && isspace(*cp); ++cp)
		;
	    if (!*cp || *cp == '#')
		continue;

	    pip(cp);
	}
    }
    
    return 0;
}
