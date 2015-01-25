#ifndef ADMMLASSO_H
#define ADMMLASSO_H

#include "ADMMBase.h"

// minimize  1/2 * ||y - X * beta||^2 + lambda * ||beta||_1
//
// In ADMM form,
//   minimize f(x) + g(z)
//   s.t. Ax + z = c
//
// x => beta
// z => -X * beta
// A => X
// b => y
// c => 0
// f(x) => lambda * ||x||_1
// g(z) => 1/2 * ||z + b||^2
class ADMMLasso: public ADMMBase<Eigen::SparseVector<double>, Eigen::VectorXd>
{
private:
    typedef Eigen::MatrixXd MatrixXd;
    typedef Eigen::VectorXd VectorXd;
    typedef Eigen::ArrayXd ArrayXd;
    typedef Eigen::SparseVector<double> SparseVector;

    const MatrixXd *datX;         // data matrix
    const VectorXd *datY;         // response vector
    double sprad;                 // spectral radius of X'X
    double lambda;                // L1 penalty

    VectorXd cache_Ax;            // cache Ax
    
    virtual void A_mult(VectorXd &res, SparseVector &x) // x -> Ax
    {
        res = (*datX) * x;
    }
    virtual void At_mult(VectorXd &res, VectorXd &y) // y -> A'y
    {
        res = (*datX).transpose() * y;
    }
    virtual void B_mult (VectorXd &res, VectorXd &z) // z -> Bz
    {
        res = z;
    }  
    virtual double c_norm() { return 0.0; } // ||c||_2
    virtual void next_residual(VectorXd &res)
    {
        res.noalias() = cache_Ax + aux_z;
    }
    
    virtual void next_x(SparseVector &res)
    {
        double gamma = 2 * rho + sprad;
        VectorXd vec = cache_Ax + aux_z + dual_y / rho;
        vec = -(*datX).transpose() * vec / gamma;
        vec += main_x;
        soft_threshold(res, vec, lambda / (rho * gamma));
    }
    virtual void next_z(VectorXd &res)
    {
        cache_Ax = (*datX) * main_x;

        res.noalias() = ((*datY) + dual_y + rho * cache_Ax) / (-1 - rho);
    }
    virtual void rho_changed_action() {}
    // calculating eps_primal
    virtual double compute_eps_primal()
    {
        double r = std::max(cache_Ax.norm(), aux_z.norm());
        return r * eps_rel + sqrt(double(dim_dual)) * eps_abs;
    }
    
public:
    ADMMLasso(const MatrixXd &datX_, const VectorXd &datY_,
              double sprad_,
              double eps_abs_ = 1e-6,
              double eps_rel_ = 1e-6) :
        ADMMBase(datX_.cols(), datX_.rows(), datX_.rows(),
                 eps_abs_, eps_rel_),
        datX(&datX_), datY(&datY_),
        sprad(sprad_),
        cache_Ax(dim_dual)
    {
        cache_Ax.setZero();
    }

    virtual double lambda_max() { return ((*datX).transpose() * (*datY)).array().abs().maxCoeff(); }

    // init() is a cold start for the first lambda
    virtual void init(double lambda_, double rho_)
    {
        main_x.setZero();
        aux_z.setZero();
        dual_y.setZero();
        lambda = lambda_;
        rho = rho_;
        eps_primal = 0.0;
        eps_dual = 0.0;
        resid_primal = 9999;
        resid_dual = 9999;

        rho_changed_action();
    }
    // when computing for the next lambda, we can use the
    // current main_x, aux_z, dual_y and rho as initial values
    virtual void init_warm(double lambda_)
    {
        lambda = lambda_;
        eps_primal = 0.0;
        eps_dual = 0.0;
        resid_primal = 9999;
        resid_dual = 9999;
    }

    static void soft_threshold(SparseVector &res, VectorXd &vec, const double &penalty)
    {
        res.reserve(vec.size() / 2);

        double *ptr = vec.data();
        for(int i = 0; i < vec.size(); i++)
        {
            if(ptr[i] > penalty)
            {
                res.insertBack(i) = ptr[i] - penalty;
            }
            else if(ptr[i] < -penalty)
            {
                res.insertBack(i) = ptr[i] + penalty;
            }
        }
    }
};



#endif // ADMMLASSO_H