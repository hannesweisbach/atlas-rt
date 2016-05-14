#pragma once

struct matrix {
  double **matrix;         // matrix data, indexed columns first, rows second
  size_t   columns;        // column count
};

struct llsp_s {
  size_t        metrics;   // metrics count
  double       *data;      // pointer to the malloc'ed data block, matrix is transposed
  struct matrix full;      // pointers to the individual columns for easy column dropping
  struct matrix sort;      // matrix columns with dropped metrics moved to the right
  struct matrix good;      // reduced matrix with low-contribution columns dropped
  double        last_measured;
  double        result[];  // the resulting coefficients
};
