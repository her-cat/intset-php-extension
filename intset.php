<?php

if(!extension_loaded('intset')) {
	dl('php_intset.' . PHP_SHLIB_SUFFIX);
}

$intset = create_intset();

var_dump($intset);

var_dump(intset_add($intset, 1));
var_dump(intset_find($intset, 1));
var_dump(intset_remove($intset, 1));
var_dump(intset_find($intset, 1));


intset_add($intset, 1);
intset_add($intset, 2);
intset_add($intset, 3);

var_dump(intset_random($intset));
var_dump(intset_get($intset, 1));
var_dump(intset_len($intset));
var_dump(intset_blob_len($intset));