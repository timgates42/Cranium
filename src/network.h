#include "std_includes.h"
#include "matrix.h"
#include "function.h"
#include "layer.h"

#ifndef NETWORK_H
#define NETWORK_H

typedef struct Network_ {
    int numLayers;
    Layer** layers;
    int numConnections;
    Connection** connections;
} Network;

// constructor to create a network given sizes and functions
// hiddenSizes is an array of sizes, where hiddenSizes[i] is the size
// of the ith hidden layer
// hiddenActivations is an array of activation functions,
// where hiddenActivations[i] is the function of the ith hidden layer
Network* createNetwork(int numFeatures, int numHiddenLayers, int* hiddenSizes, void (**hiddenActivations)(Matrix*), int numClasses, void (*outputActivation)(Matrix*));

// will propagate input through entire network
// result will be stored in input field of last layer
// input should be a matrix where each row is an input
void forwardPass(Network* network, Matrix* input);

// calculate the cross entropy loss between two datasets with 
// optional regularization (must provide network if using regularization)
double crossEntropyLoss(Network* network, Matrix* prediction, Matrix* actual, double regularizationStrength);

// returns indices corresponding to highest-probability classes for each
// example previously inputted
// assumes final output is in the output layer of network
int* predict(Network* network);

// return accuracy (num_correct / num_total) of network on predictions
double accuracy(Network* network, Matrix* data, Matrix* classes);

// frees network, its layers, and its connections
void destroyNetwork(Network* network);

// write network configuration to a file
void saveNetwork(Network* network, char* path);

// read network configuration from a file
Network* readNetwork(char* path);


/*
    Begin functions.
*/

Network* createNetwork(int numFeatures, int numHiddenLayers, int* hiddenSizes, void (**hiddenActivations)(Matrix*), int numClasses, void (*outputActivation)(Matrix*)){
    assert(numFeatures > 0 && numHiddenLayers >= 0 && numClasses > 0);
    Network* network = (Network*)malloc(sizeof(Network));
    
    network->numLayers = 2 + numHiddenLayers;
    Layer** layers = (Layer**)malloc(sizeof(Layer*) * network->numLayers);
    int i;
    for (i = 0; i < network->numLayers; i++){
        // create input
        if (i == 0){
            layers[i] = createLayer(INPUT, numFeatures, NULL);
        }
        //create output
        else if (i == network->numLayers - 1){
            layers[i] = createLayer(OUTPUT, numClasses, outputActivation);
        }
        // create hidden layer
        else{
            layers[i] = createLayer(HIDDEN, hiddenSizes[i - 1], hiddenActivations[i - 1]);
        }
    }
    network->layers = layers;

    network->numConnections = network->numLayers - 1;
    Connection** connections = (Connection**)malloc(sizeof(Connection*) * network->numConnections);
    for (i = 0; i < network->numConnections; i++){
        connections[i] = createConnection(network->layers[i], network->layers[i + 1]);
        initializeConnection(connections[i]);
    }
    network->connections = connections;

    return network;
}

void forwardPass(Network* network, Matrix* input){
    assert(input->cols == network->layers[0]->input->cols);
    destroyMatrix(network->layers[0]->input);
    network->layers[0]->input = copy(input);
    int i;
    Matrix* tmp,* tmp2;
    for (i = 0; i < network->numConnections; i++){
        tmp = multiply(network->layers[i]->input, network->connections[i]->weights);
        tmp2 = addToEachRow(tmp, network->connections[i]->bias);
        destroyMatrix(network->connections[i]->to->input);
        network->connections[i]->to->input = tmp2;
        destroyMatrix(tmp);
        activateLayer(network->connections[i]->to);
    }
}

// matrixes of size [num examples] x [num classes]
double crossEntropyLoss(Network* network, Matrix* prediction, Matrix* actual, double regularizationStrength){
    assert(prediction->rows == actual->rows);
    assert(prediction->cols == actual->cols);
    double total_err = 0;
    int i, j, k;
    for (i = 0; i < prediction->rows; i++){
        double cur_err = 0;
        for (j = 0; j < prediction->cols; j++){
            cur_err += actual->data[i][j] * log(MAX(DBL_MIN, prediction->data[i][j]));
        }
        total_err += cur_err;
    }
    double reg_err = 0;
    if (network != NULL){
        for (i = 0; i < network->numConnections; i++){
            Matrix* weights = network->connections[i]->weights;
            for (j = 0; j < weights->rows; j++){
                for (k = 0; k < weights->cols; k++){
                    reg_err += weights->data[j][k] * weights->data[j][k];
                }
            }
        }
    }
    return ((-1.0 / actual->rows) * total_err) + (regularizationStrength * .5 * reg_err);
}

int* predict(Network* network){
    int i, j, max;
    Layer* outputLayer = network->layers[network->numLayers - 1];
    int* predictions = (int*)malloc(sizeof(int) * outputLayer->input->rows);
    for (i = 0; i < outputLayer->input->rows; i++){
        max = 0;
        for (j = 1; j < outputLayer->size; j++){
            if (outputLayer->input->data[i][j] > outputLayer->input->data[i][max]){
                max = j;
            }
        }
        predictions[i] = max;
    }
    return predictions;
}

double accuracy(Network* network, Matrix* data, Matrix* classes){
    assert(data->rows == classes->rows);
    assert(data->cols == network->layers[network->numLayers - 1]->size);
    forwardPass(network, data);
    int* predictions = predict(network);
    double numCorrect = 0;
    int i;
    for (i = 0; i < data->rows; i++){
        if (classes->data[i][predictions[i]] == 1){
            numCorrect++;
        }
    }
    free(predictions);
    return numCorrect / classes->rows;
}

void destroyNetwork(Network* network){
    int i;
    for (i = 0; i < network->numLayers; i++){
        destroyLayer(network->layers[i]);
    }
    for (i = 0; i < network->numConnections; i++){
        destroyConnection(network->connections[i]);
    }
    free(network->layers);
    free(network->connections);
    free(network);
}

// serializes in order: sizes --> weights --> bias
void saveNetwork(Network* network, char* path){
    FILE* fp = fopen(path, "w");
    int i, j, k;

    // serialize number of layers
    fprintf(fp, "%d\n", network->numLayers);

    // serialize layer sizes
    for (i = 0; i < network->numLayers; i++){
        fprintf(fp, "%d\n", network->layers[i]->size);
    }

    // serialize hidden functions
    for (i = 0; i < network->numLayers - 2; i++){
        if (network->layers[1 + i]->activation == sigmoid){
            fprintf(fp, "sigmoid\n");
        }
        else if (network->layers[1 + i]->activation == relu){
            fprintf(fp, "relu\n");
        }
        else{
            fprintf(fp, "tanH\n");
        }
    }

    // serialize output function
    if (network->layers[network->numLayers - 1]->activation == softmax){
        fprintf(fp, "softmax\n");
    }

    // serialize weights in row-major ordering
    for (k = 0; k < network->numConnections; k++){
        Connection* con = network->connections[k];
        for (i = 0; i < con->weights->rows; i++){
            for (j = 0; j < con->weights->cols; j++){
                fprintf(fp, "%la\n", con->weights->data[i][j]);
            }
        }
    }

    // serialize bias
    for (k = 0; k < network->numConnections; k++){
        Connection* con = network->connections[k];
        for (i = 0; i < con->bias->cols; i++){
            fprintf(fp, "%la\n", con->bias->data[0][i]);
        }
    }

    fclose(fp);
}

Network* readNetwork(char* path){
    FILE* fp = fopen(path, "r");
    int i, j, k;
    char buf[50];

    // get number of layers
    int numLayers;
    fgets(buf, 50, fp);
    sscanf(buf, "%d", &numLayers);
    memset(&buf[0], 0, 50);

    // get layer sizes
    int layerSizes[numLayers];
    for (i = 0; i < numLayers; i++){
        fgets(buf, 50, fp);
        sscanf(buf, "%d", &layerSizes[i]);
        memset(&buf[0], 0, 50);
    }

    // get all activation functions
    Activation funcs[numLayers - 1];
    char funcString[50];
    for (i = 0; i < numLayers - 1; i++){
        fgets(buf, 50, fp);
        sscanf(buf, "%s", funcString);
        if (strcmp(funcString, "sigmoid") == 0){
            funcs[i] = sigmoid;
        }
        else if (strcmp(funcString, "relu") == 0){
            funcs[i] = relu;
        }
        else if (strcmp(funcString, "tanH") == 0){
            funcs[i] = tanH;
        }
        else{
            funcs[i] = softmax;
        }
        memset(&buf[0], 0, 50);
    }

    // construct network structure
    Network* network;
    int inputSize = layerSizes[0];
    int outputSize = layerSizes[numLayers - 1];
    int numHiddenLayers = numLayers - 2;
    Activation outputFunc = funcs[numLayers - 2];
    if (numHiddenLayers > 0){
        int hiddenSizes[numLayers - 2];
        for (i = 0; i < numLayers - 2; i++){
            hiddenSizes[i] = layerSizes[1 + i];
        }
        Activation hiddenFuncs[numLayers - 2];
        for (i = 0; i < numLayers - 2; i++){
            hiddenFuncs[i] = funcs[i];
        }
        network = createNetwork(inputSize, numHiddenLayers, hiddenSizes, hiddenFuncs, outputSize, outputFunc);
    }
    else{
        network = createNetwork(inputSize, 0, NULL, NULL, outputSize, outputFunc);
    }

    // fill in weights
    for (k = 0; k < network->numConnections; k++){
        Connection* con = network->connections[k];
        for (i = 0; i < con->weights->rows; i++){
            for (j = 0; j < con->weights->cols; j++){
                fgets(buf, 50, fp);
                sscanf(buf, "%la", &con->weights->data[i][j]);
                memset(&buf[0], 0, 50);
            }
        }
    }

    // fill in bias
    for (k = 0; k < network->numConnections; k++){
        Connection* con = network->connections[k];
        for (i = 0; i < con->bias->cols; i++){
            fgets(buf, 50, fp);
            sscanf(buf, "%la", &con->bias->data[0][i]);
            memset(&buf[0], 0, 50);
        }
    }

    fclose(fp);
    return network;
}

#endif