--TEST--
Check intset basic functions
--SKIPIF--
<?php if (!extension_loaded("intset")) print "skip"; ?>
--FILE--
<?php
$intset = create_intset();

var_dump((bool)$intset);

var_dump(intset_add($intset, 1));
var_dump(intset_find($intset, 1));
var_dump(intset_remove($intset, 1));
var_dump(intset_find($intset, 1));


intset_add($intset, 1);
intset_add($intset, 2);
intset_add($intset, 3);

var_dump(intset_get($intset, 1));
var_dump(intset_len($intset));
var_dump(intset_blob_len($intset));
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
int(2)
int(3)
int(14)
