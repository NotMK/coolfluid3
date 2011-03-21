// OpenCL Kernel
char* GPUSource =
{
"    __kernel void interpolation(__global float* PHI,\n"
"                                __global float* A, __global float* A_ksi, __global float* A_eta,\n"
"                                __global float* X_real, __global float* U_real,\n"
"                                unsigned int shape, unsigned int quad, unsigned int dim, unsigned int elements,\n"
"                                __local float* X_quad, __local float* X_ksi, __local float* X_eta,\n"
"                                __local float* jacobi_quad, __local float* sum_phi_quad , __local float* LU_quad )\n"
"    {\n"
"    for( unsigned int tx = get_group_id(0); tx <elements; tx += get_num_groups(0) )\n"
"    {\n"
"         // interpolation to quadrature points\n"
"         \n"
"         for(unsigned int j = get_local_id(0);j< quad;j+= get_local_size(0) )\n"
"         {\n"
"               for( unsigned int k = 0; k < dim; k++ )\n"
"               {\n"
"                   unsigned int elemC = j * dim + k;\n"
"                   \n"
"                   float value_int = 0;\n"
"                   float value_ksi = 0;\n"
"                   float value_eta = 0;\n"
"                   \n"
"                   for( unsigned int l = 0; l < shape; l++ )\n"
"                   {\n"
"                       unsigned int elemA = j * shape + l;\n"
"                       unsigned int elemB = tx * ( dim * shape ) + l * dim + k;\n"
"                       \n"
"                       value_int += A[elemA]     * X_real[elemB];\n"
"                       value_ksi += A_ksi[elemA] * X_real[elemB];\n"
"                       value_eta += A_eta[elemA] * X_real[elemB];\n"
"                   }\n"
"                   X_quad[elemC] = value_int;\n"
"                   X_ksi[elemC]  = value_ksi;\n"
"                   X_eta[elemC]  = value_eta;\n"
"               }\n"
"        }\n"
"      barrier(CLK_LOCAL_MEM_FENCE); \n"
"                                    \n"
"          for(unsigned int j = get_local_id(0);j< quad;j+= get_local_size(0) )\n"
"          {\n"
"                unsigned int elemC = j;\n"
"                unsigned int elemA = j * dim;\n"
"                float value = X_ksi[elemA] * X_eta[elemA+1] - X_eta[elemA] * X_ksi[elemA+1];\n"
"                if (fabs(value) < 1e-6) value = 1e-6;"
"                \n"
"                jacobi_quad[elemC] = value;\n"
"          }\n"
"           \n"
"         for(unsigned int j = get_local_id(0);j< quad;j+= get_local_size(0) ) \n"
"         { \n"
"                unsigned int elemC = j; \n"
"                float value = 0; \n"
"                float value1 = 0; \n"
"                \n"
"                for( int k = 0; k < shape; k++ ) \n"
"                { \n"
"                    unsigned int elemMatrix = j * shape + k; \n"
"                    unsigned int elemA      = j * dim; \n"
"                    unsigned int elemB	= tx * ( dim * shape ) + k; \n"
"                    \n"
"                    float phiX =  A_ksi[elemMatrix] * X_eta[elemA+1] - A_eta[elemMatrix] * X_ksi[elemA+1]; \n"
"                    float phiY = -A_ksi[elemMatrix] * X_eta[elemA]   + A_eta[elemMatrix] * X_ksi[elemA]; \n"
"                    \n"
"                    value += 1.0 * ( X_quad[elemA+1] * phiX  - X_quad[elemA] * phiY ); \n"
"                    value1 += 1.0 * ( -X_quad[elemA+1] * phiX  - X_quad[elemA] * phiY )*U_real[elemB]; \n"
"                } \n"
"                value = value / jacobi_quad[j];  if( fabs( value ) < 1e-6 ) value = 1e-6; \n"
"                value1 = value1 / jacobi_quad[j]; if( fabs( value1 ) < 1e-6 ) value1 = 1e-6;  \n"
"                sum_phi_quad[elemC] = value; \n"
"                LU_quad[elemC] = value1; \n"
"          } \n"
"            \n"
"            \n"
"         for(unsigned int j = get_local_id(0);j< shape;j+= get_local_size(0) ) \n"
"          {   \n"
"              unsigned int elemC = tx *  shape + j; \n"
"              float value = 0; \n"
"              float jacobi = 0; \n"
"          \n"
"              for( int k = 0; k < quad; k++ ) \n"
"              {  \n"
"                  unsigned int elemMatrix = k * shape + j; \n"
"                  unsigned int elemA      = k * dim; \n"
"              \n"
"                  float phiX =  A_ksi[elemMatrix] * X_eta[elemA+1] - A_eta[elemMatrix] * X_ksi[elemA+1]; \n"
"                  float phiY = -A_ksi[elemMatrix] * X_eta[elemA]   + A_eta[elemMatrix] * X_ksi[elemA]; \n"
"              \n"
"                  value += 1.0  / jacobi_quad[k] * ( X_quad[elemA+1] * phiX  - X_quad[elemA] * phiY ) * LU_quad[k] /  sum_phi_quad[k]; \n"
"                  jacobi += jacobi_quad[k]; \n"
"              } \n"
"              PHI[elemC] = value*jacobi; \n"
"           }     \n"
"     }\n"
"  }\n"
};
