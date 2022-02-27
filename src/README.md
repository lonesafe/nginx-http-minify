# HTTP Minify module for Nginx

## Introduction 

This is a module for nginx. The module is ported a function from [The JavaScript Minifier of Douglas Crockford](http://javascript.crockford.com/jsmin.html). It follows the same
pattern for enabling the minify as gzip module in nginx. 

## Configuration example

    location /static/css/ {
        minify on;
    }
        
    location /static/js/ {
        concat on;
    }

## Module directives

**minify** `on` | `off`

**default:** `minify off`

**context:** `http, server, location`

It enables the minify in a given context.


<br/>
<br/>

**minify_types** `MIME types`

**default:** `minify_types: text/css application/x-javascript`

**context:** `http, server, location`

Defines the [MIME types](http://en.wikipedia.org/wiki/MIME_type) which
can be concatenated in a given context.

## Unit Test

The test module is test-nginx from [agentzh project](https://github.com/agentzh/test-nginx). There are two files for minify module in the directory test/t:

**test_minify_js.t** is the unit test file for jsmin

**test_minify_css.t** is the unit test file for cssmin

###Run test

1 install the test-nginx module:

     cd test
     perl Makefile.PL
     make
     make install

2 run test:

     PATH=/usr/local/nginx/sbin/:$PATH prove t/test_minify_js.t

     PATH=/usr/local/nginx/sbin/:$PATH prove t/test_minify_css.t


   


