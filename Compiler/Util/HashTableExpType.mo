encapsulated package HashTableExpType "
  This file is an extension to OpenModelica.

  Copyright (c) 2007 MathCore Engineering AB

  All rights reserved.

  file:        HashTableExpType.mo
  package:     HashTableExpType
  description: DAE.Exp to DAE.ExpType

  RCS: $Id: Util.mo 8498 2011-04-05 12:17:00Z sjoelund.se $

  "
  
/* Below is the instance specific code. For each hashtable the user must define:

Key       - The key used to uniquely define elements in a hashtable
Value     - The data to associate with each key
hashFunc   - A function that maps a key to a positive integer.
keyEqual   - A comparison function between two keys, returns true if equal.
*/

/* HashTable instance specific code */

public import BaseHashTable;
public import DAE;
protected import ExpressionDump;
protected import Types;
protected import System;

public type Key = DAE.Type;
public type Value = DAE.ExpType;

public type HashTableCrefFunctionsType = tuple<FuncHashCref,FuncCrefEqual,FuncCrefStr,FuncExpStr>;
public type HashTable = tuple<
  array<list<tuple<Key,Integer>>>,
  tuple<Integer,Integer,array<Option<tuple<Key,Value>>>>,
  Integer,
  Integer,
  HashTableCrefFunctionsType
>;

partial function FuncHashCref
  input Key cr;
  input Integer mod;
  output Integer res;
end FuncHashCref;

partial function FuncCrefEqual
  input Key cr1;
  input Key cr2;
  output Boolean res;
end FuncCrefEqual;

partial function FuncCrefStr
  input Key cr;
  output String res;
end FuncCrefStr;

partial function FuncExpStr
  input Value exp;
  output String res;
end FuncExpStr;

protected function hashFunc
"Calculates a hash value for Key"
  input Key key;
  input Integer mod;
  output Integer res;
protected
  String keystr;
algorithm
  //keystr := Types.unparseType(key);
  res := valueHashMod(key,mod); //System.stringHashDjb2Mod(keystr,mod);
end hashFunc;

public function emptyHashTable
"
  Returns an empty HashTable.
  Using the bucketsize 1000 and array size 100.
"
  output HashTable hashTable;
algorithm
  hashTable := emptyHashTableSized(BaseHashTable.defaultBucketSize);
end emptyHashTable;

public function emptyHashTableSized
"
  Returns an empty HashTable.
  Using the bucketsize size and arraysize size/10.
"
  input Integer size;
  output HashTable hashTable;
algorithm
  hashTable := BaseHashTable.emptyHashTableWork(size,intDiv(size,10),(hashFunc,valueEq,Types.unparseType,ExpressionDump.typeString));
end emptyHashTableSized;

end HashTableExpType;
