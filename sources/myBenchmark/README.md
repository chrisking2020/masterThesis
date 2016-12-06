# myBenchmark

This is an example benchmark for DAEDAL project.


## Details

struct Person contains 3 attributes: **ID**, **attr** and **contactPerson**, all null initialized.

* **ID** is of type unsigned int, supposed to be unique to identify a Person
* **attr** is of type int, descriptor on some feature
* **contactPerson** is of type pointer to struct Person, pointing to some other Person but not oneself

[First loop in main]: Unique **ID** is generated to all entries in a Person vector.

[Second loop in main]: Random **contactPerson** is assigned to every entry in the Person Vector.

[Third loop in main]: **attr** is changed to 1 if the contactPerson of this Person's contactPerson is this Person oneself.

[Fourth loop in main]: printout all attributes for all entries in Person vector.

There are 5 levels of indirections in the third loop, which is of interest in this benchmark.

Please notice that there is one *#pragma clang loop* notation before the third loop. This is from LLVM loop vectorizer used to improve performance for hot loops. Right now *width* argument (1337) is not actually custom appliable.

### Parameters

Users can specify the size of Person vector as well as the seed used for random.
If none is specified, default values will be applied.
