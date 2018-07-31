# Assignment 3 - Record Manager

## tables

You probably will have to reserve one or more pages of a page file to store table information pages, e.g. the schema of the table.

This header defines basic data structures for schemas, tables, records, record ids (RIDs), and values. Furthermore, this header defines functions for serializing these data structures as strings. The serialization functions are provided (rm_serializer.c)