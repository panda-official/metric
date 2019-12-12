#ifndef NETWORK_H_
#define NETWORK_H_

#include <vector>
#include <stdexcept>
#include <utility>
#include <random>

#include "../../../3rdparty/blaze/Math.h"

#include "Layer.h"
#include "Output.h"
#include "Callback.h"
#include "Utils/Random.h"

namespace dnn
{


///
/// \defgroup Network Neural Network Model
///

///
/// \ingroup Network
///
/// This class represents a neural network model that typically consists of a
/// number of hidden layers and an output layer. It provides functions for
/// network building, model fitting, and prediction, etc.
///
template <typename Scalar>
class Network
{
    private:
		using Matrix = blaze::DynamicMatrix<Scalar, blaze::columnMajor>;

        std::mt19937                 m_default_rng;      // Built-in std::mt19937
        std::mt19937&                m_rng;              // Reference to the std::mt19937 provided by the user,
												// otherwise reference to m_default_rng
        std::shared_ptr<Output<Scalar>>             m_output;           // The output layer
        Callback<Scalar>                           m_default_callback; // Default callback function
        std::shared_ptr<Callback<Scalar>>           m_callback;         // Points to user-provided callback function,
												// otherwise points to m_default_callback

        // Check dimensions of layers
        void check_unit_sizes() const
        {
            const int nlayer = num_layers();

            if (nlayer <= 1)
            {
                return;
            }

            for (int i = 1; i < nlayer; i++)
            {
                if (m_layers[i]->in_size() != m_layers[i - 1]->out_size())
                {
                    throw std::invalid_argument("Unit sizes do not match");
                }
            }
        }

        // Let each layer compute its output
        void forward(const Matrix& input)
        {
            const int nlayer = num_layers();

            if (nlayer <= 0)
            {
                return;
            }

            // First layer
            if (input.rows() != m_layers[0]->in_size())
            {
                throw std::invalid_argument("Input data have incorrect dimension");
            }

            m_layers[0]->forward(input);

            // The following layers
            for (int i = 1; i < nlayer; i++)
            {
                m_layers[i]->forward(m_layers[i - 1]->output());
            }
        }

        // Let each layer compute its gradients of the parameters
        // target has two versions: Matrix and RowVectorXi
        // The RowVectorXi version is used in classification problems where each
        // element is a class label
        template <typename TargetType>
        void backprop(const Matrix& input, const TargetType& target)
        {
            const int nlayer = num_layers();

            if (nlayer <= 0)
            {
                return;
            }

            std::shared_ptr<Layer<Scalar>> first_layer = m_layers[0];
            std::shared_ptr<Layer<Scalar>> last_layer = m_layers[nlayer - 1];
            // Let output layer compute back-propagation data
            m_output->check_target_data(target);
            m_output->evaluate(last_layer->output(), target);

            // If there is only one hidden layer, "prev_layer_data" will be the input data
            if (nlayer == 1)
            {
                first_layer->backprop(input, m_output->backprop_data());
                return;
            }

            // Compute gradients for the last hidden layer
            last_layer->backprop(m_layers[nlayer - 2]->output(), m_output->backprop_data());

            // Compute gradients for all the hidden layers except for the first one and the last one
            for (int i = nlayer - 2; i > 0; i--)
            {
                m_layers[i]->backprop(m_layers[i - 1]->output(),
                                      m_layers[i + 1]->backprop_data());
            }

            // Compute gradients for the first layer
            first_layer->backprop(input, m_layers[1]->backprop_data());
        }

        // Update parameters
        void update(Optimizer<Scalar>& opt)
        {
            const int nlayer = num_layers();

            if (nlayer <= 0)
            {
                return;
            }

            for (int i = 0; i < nlayer; i++)
            {
                m_layers[i]->update(opt);
            }
        }

    public:

		std::vector<std::shared_ptr<Layer<Scalar>>> m_layers;           // Pointers to hidden layers
	///
        /// Default constructor that creates an empty neural network
        ///
        Network() :
            m_default_rng(1),
            m_rng(m_default_rng),
            m_output(NULL),
            m_default_callback(),
            m_callback(NULL)
        {}

        ///
        /// Constructor with a user-provided random number generator
        ///
        /// \param rng A user-provided random number generator object that inherits
        ///            from the default std::mt19937 class.
        ///
        Network(std::mt19937& rng) :
            m_default_rng(1),
            m_rng(rng),
            m_output(NULL),
            m_default_callback(),
            m_callback(NULL)
        {}

        ///
        /// Destructor that frees the added hidden layers and output layer
        ///
        ~Network()   {}

        ///
        /// Add a hidden layer to the neural network
        ///
        /// \param layer A pointer to a Layer object, typically constructed from
        ///              layer classes such as FullyConnected and Convolutional.
        ///              **NOTE**: the pointer will be handled and freed by the
        ///              network object, so do not delete it manually.
        ///
        template<typename T>
        void addLayer(const T &layer)
        {
            m_layers.push_back(std::make_shared<T>(layer));
        }

        ///
        /// Set the output layer of the neural network
        ///
        /// \param output A pointer to an Output object, typically constructed from
        ///               output layer classes such as RegressionMSE and MultiClassEntropy.
        ///               **NOTE**: the pointer will be handled and freed by the
        ///               network object, so do not delete it manually.
        ///
        template<typename T>
        void setOutput(const T &output)
        {
            m_output = std::make_shared<T>(output);
        }

        ///
        /// Number of hidden layers in the network
        ///
        int num_layers() const
        {
            return m_layers.size();
        }

        ///
        /// Get the list of hidden layers of the network
        ///
        /*std::vector<const std::shared_ptr<Layer>> get_layers() const
        {
            const int nlayer = num_layers();
            std::vector<const std::shared_ptr<Layer>> layers(nlayer);
            std::copy(m_layers.begin(), m_layers.end(), layers.begin());
            return layers;
        }*/

        ///
        /// Get the output layer
        ///
        const Output<Scalar>* get_output() const
        {
            return m_output.get();
        }

        ///
        /// Set the callback function that can be called during model fitting
        ///
        /// \param callback A user-provided callback function object that inherits
        ///                 from the default Callback class.
        ///
        template<typename T>
        void setCallback(const T &callback)
        {
            m_callback = std::make_shared<T>(callback);
        }
        ///
        /// Set the default silent callback function
        ///
        void set_default_callback()
        {
            m_callback = &m_default_callback;
        }

        ///
        /// Initialize layer parameters in the network using normal distribution
        ///
        /// \param mu    Mean of the normal distribution.
        /// \param sigma Standard deviation of the normal distribution.
        /// \param seed  Set the random seed of the %std::mt19937 if `seed > 0`, otherwise
        ///              use the current random state.
        ///
        void init(const Scalar& mu = Scalar(0), const Scalar& sigma = Scalar(0.01),
                  int seed = -1)
        {
            check_unit_sizes();

            if (seed > 0)
            {
                m_rng.seed(seed);
            }

            const int nlayer = num_layers();

            for (int i = 0; i < nlayer; i++)
            {
                m_layers[i]->init(mu, sigma, m_rng);
            }
        }

        ///
        /// Get the serialized layer parameters
        ///
        std::vector< std::vector<Scalar>> get_parameters() const
        {
            const int nlayer = num_layers();
            std::vector< std::vector<Scalar>> res;
            res.reserve(nlayer);

            for (int i = 0; i < nlayer; i++)
            {
                res.push_back(m_layers[i]->get_parameters());
            }

            return res;
        }

        ///
        /// Set the layer parameters
        ///
        /// \param param Serialized layer parameters
        ///
        void set_parameters(const std::vector< std::vector<Scalar>>& param)
        {
            const int nlayer = num_layers();

            if (static_cast<int>(param.size()) != nlayer)
            {
                throw std::invalid_argument("Parameter size does not match");
            }

            for (int i = 0; i < nlayer; i++)
            {
                m_layers[i]->set_parameters(param[i]);
            }
        }

        ///
        /// Get the serialized derivatives of layer parameters
        ///
        std::vector< std::vector<Scalar>> get_derivatives() const
        {
            const int nlayer = num_layers();
            std::vector< std::vector<Scalar>> res;
            res.reserve(nlayer);

            for (int i = 0; i < nlayer; i++)
            {
                res.push_back(m_layers[i]->get_derivatives());
            }

            return res;
        }

/*
        ///
        /// Debugging tool to check parameter gradients
        ///
        template <typename TargetType>
        void check_gradient(const Matrix& input, const TargetType& target, int npoints,
                            int seed = -1)
        {
            if (seed > 0)
            {
                m_rng.seed(seed);
            }

            this->forward(input);
            this->backprop(input, target);
            std::vector< std::vector<Scalar>> param = this->get_parameters();
            std::vector< std::vector<Scalar>> deriv = this->get_derivatives();
            const Scalar eps = 1e-5;
            const int nlayer = deriv.size();

            for (int i = 0; i < npoints; i++)
            {
                // Randomly select a layer
                const int layer_id = int(m_rng() * nlayer);
                // Randomly pick a parameter, note that some layers may have no parameters
                const int nparam = deriv[layer_id].size();

                if (nparam < 1)
                {
                    continue;
                }

                const int param_id = int(m_rng() * nparam);
                // Turbulate the parameter a little bit
                const Scalar old = param[layer_id][param_id];
                param[layer_id][param_id] -= eps;
                this->set_parameters(param);
                this->forward(input);
                this->backprop(input, target);
                const Scalar loss_pre = m_output->loss();
                param[layer_id][param_id] += eps * 2;
                this->set_parameters(param);
                this->forward(input);
                this->backprop(input, target);
                const Scalar loss_post = m_output->loss();
                const Scalar deriv_est = (loss_post - loss_pre) / eps / 2;
                std::cout << "[layer " << layer_id << ", param " << param_id <<
                          "] deriv = " << deriv[layer_id][param_id] << ", est = " << deriv_est <<
                          ", diff = " << deriv_est - deriv[layer_id][param_id] << std::endl;
                param[layer_id][param_id] = old;
            }

            // Restore original parameters
            this->set_parameters(param);
        }
*/

        ///
        /// Fit the model based on the given data
        ///
        /// \param opt        An object that inherits from the Optimizer class, indicating the optimization algorithm to use.
        /// \param x          The predictors. Each column is an observation.
        /// \param y          The response variable. Each column is an observation.
        /// \param batch_size Mini-batch size.
        /// \param epoch      Number of epochs of training.
        /// \param seed       Set the random seed of the %std::mt19937 if `seed > 0`, otherwise
        ///                   use the current random state.
        ///
        template <typename DerivedX, typename DerivedY>
        bool fit(Optimizer<Scalar>& opt, const DerivedX& x,
                 const DerivedY& y,
                 int batch_size, int epoch, int seed = -1)
        {
            // We do not directly use PlainObjectX since it may be row-majored if x is passed as mat.transpose()
            // We want to force XType and YType to be column-majored
            const int nlayer = num_layers();

            if (nlayer <= 0)
            {
                return false;
            }

            // Reset optimizer
            opt.reset();

            // Create shuffled mini-batches
            if (seed > 0)
            {
                m_rng.seed(seed);
            }

            std::vector<DerivedX> x_batches;
            std::vector<DerivedY> y_batches;
            const int nbatch = internal::create_shuffled_batches(x, y, batch_size, m_rng,
                               x_batches, y_batches);
            // Set up callback parameters
            m_callback->m_nbatch = nbatch;
            m_callback->m_nepoch = epoch;

            // Iterations on the whole data set
            for (int k = 0; k < epoch; k++)
            {
                m_callback->m_epoch_id = k;

                // Train on each mini-batch
                for (int i = 0; i < nbatch; i++)
                {
                    m_callback->m_batch_id = i;
                    m_callback->pre_training_batch(this, x_batches[i], y_batches[i]);
                    this->forward(x_batches[i]);
                    this->backprop(x_batches[i], y_batches[i]);
                    this->update(opt);
                    m_callback->post_training_batch(this, x_batches[i], y_batches[i]);
                }
            }

            return true;
        }

        ///
        /// Use the fitted model to make predictions
        ///
        /// \param x The predictors. Each column is an observation.
        ///
        Matrix predict(const Matrix& x)
        {
            const int nlayer = num_layers();

            if (nlayer <= 0)
            {
                return Matrix();
            }

            this->forward(x);
            return m_layers[nlayer - 1]->output();
        }
};


} // namespace dnn


#endif /* NETWORK_H_ */
