#include "ns3/core-module.h"
#include "ns3/wifi-module.h"
#include "ns3/error-rate-model-module.h"
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <utility>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiErrorRateModelValidation");

/**
 * \brief Calculates the Frame Success Rate (FSR) given a Bit Error Rate (BER) and frame size.
 * \param ber The Bit Error Rate (between 0.0 and 1.0).
 * \param bitsPerFrame The number of bits in the frame.
 * \return The calculated Frame Success Rate (FSR).
 *
 * FSR is calculated as (1 - BER)^bitsPerFrame.
 */
double CalculateFSR(double ber, uint32_t bitsPerFrame)
{
    // Clamp BER to [0, 1] to handle potential floating point inaccuracies
    if (ber < 0.0) ber = 0.0;
    if (ber > 1.0) ber = 1.0;
    return std::pow(1.0 - ber, static_cast<double>(bitsPerFrame));
}

int main(int argc, char *argv[])
{
    // Default configurable parameters
    double snrMin = 0.0;
    double snrMax = 30.0;
    double snrStep = 0.5;
    uint32_t frameSizeBytes = 1500; // Common Ethernet MTU

    // HT MCS values to be validated.
    // This selection covers 1-spatial stream HT MCS values for 20MHz bandwidth, long GI.
    // These generally correspond to readily available lookup tables for the TableBasedErrorRateModel.
    // For "every HT MCS value", this vector could be extended to include MCS 8-23 (for 2, 3 spatial streams),
    // and iterations could be added for 40MHz bandwidth and short guard interval if desired.
    std::vector<uint8_t> htMcsValues = {0, 1, 2, 3, 4, 5, 6, 7};

    // Enable logging for this component and internal error models for debugging purposes.
    LogComponentEnable("WifiErrorRateModelValidation", LOG_LEVEL_INFO);
    LogComponentEnable("ErrorRateModel", LOG_LEVEL_INFO);
    LogComponentEnable("NistErrorRateModel", LOG_LEVEL_DEBUG);
    LogComponentEnable("YansWifiErrorRateModel", LOG_LEVEL_DEBUG);
    LogComponentEnable("TableBasedErrorRateModel", LOG_LEVEL_DEBUG);

    // Command line argument setup for flexibility.
    CommandLine cmd(__FILE__);
    cmd.AddValue("snrMin", "Minimum SNR value (dB) for calculations", snrMin);
    cmd.AddValue("snrMax", "Maximum SNR value (dB) for calculations", snrMax);
    cmd.AddValue("snrStep", "Step size (dB) for SNR values", snrStep);
    cmd.AddValue("frameSizeBytes", "Size of the simulated frame in bytes", frameSizeBytes);
    cmd.Parse(argc, argv);

    uint32_t bitsPerFrame = frameSizeBytes * 8;
    NS_LOG_INFO("Configured frame size: " << frameSizeBytes << " bytes (" << bitsPerFrame << " bits)");

    // Instantiate the three requested error models.
    Ptr<ErrorRateModel> nistErrorModel = CreateObject<NistErrorRateModel>();
    Ptr<ErrorRateModel> yansErrorModel = CreateObject<YansWifiErrorRateModel>();
    Ptr<ErrorRateModel> tableErrorModel = CreateObject<TableBasedErrorRateModel>();

    // Configure the TableBasedErrorRateModel.
    // It requires paths to its lookup tables and the modulation class (802.11n for HT).
    // Ensure 'src/wifi/model/table-based-error-rates/' is accessible, typically by running from ns-3 root.
    tableErrorModel->SetAttribute("TablePaths", StringValue("src/wifi/model/table-based-error-rates/"));
    tableErrorModel->SetAttribute("ModulationClass", StringValue("802.11n"));

    // List of error models to iterate through for validation.
    std::vector<std::pair<Ptr<ErrorRateModel>, std::string>> models;
    models.push_back({nistErrorModel, "nist"});
    models.push_back({yansErrorModel, "yans"});
    models.push_back({tableErrorModel, "table"});

    // Outer loop: Iterate through each error rate model.
    for (const auto& modelEntry : models)
    {
        Ptr<ErrorRateModel> currentModel = modelEntry.first;
        std::string modelName = modelEntry.second;

        NS_LOG_INFO("--- Processing Error Model: " << modelName << " ---");

        // Middle loop: Iterate through each specified HT MCS value.
        for (uint8_t mcs : htMcsValues)
        {
            // Obtain the specific WifiMode object for the current HT MCS.
            // We use 20 MHz bandwidth and a long guard interval (false for short GI).
            // The unique name generated (e.g., "HT-MCS0-20MHz") corresponds to filenames
            // used by the TableBasedErrorRateModel.
            WifiMode htMode = WifiModeFactory::GetHtbMode(mcs, false, WifiMode::BW_20_MHZ);
            
            NS_LOG_INFO("  Processing HT MCS: " << (uint32_t)mcs << " (Unique Mode Name: " << htMode.GetUniqueName() << ")");

            // Construct a unique output filename for the Gnuplot data.
            // Example: "nist_mcs0_bw20_giN.txt"
            std::stringstream filename;
            filename << modelName << "_mcs" << (uint32_t)mcs << "_bw20_giN.txt";
            std::ofstream outFile(filename.str());

            if (!outFile.is_open())
            {
                NS_LOG_ERROR("Could not open output file: " << filename.str() << ". Check permissions or path.");
                continue; // Skip to the next MCS if file cannot be opened.
            }

            // Write a Gnuplot-compatible header to the output file.
            outFile << "# SNR (dB)\tFrame Success Rate" << std::endl;

            // Inner loop: Iterate over the specified Signal-to-Noise Ratio (SNR) range.
            for (double snr = snrMin; snr <= snrMax; snr += snrStep)
            {
                // Get the Bit Error Rate (BER) from the current error model.
                double ber = currentModel->GetErrorRate(htMode, snr);
                // Calculate the Frame Success Rate (FSR) from the BER.
                double fsr = CalculateFSR(ber, bitsPerFrame);
                // Output the SNR and FSR to the file.
                outFile << snr << "\t" << fsr << std::endl;
            }

            outFile.close();
            NS_LOG_INFO("  Data for " << modelName << " MCS " << (uint32_t)mcs << " saved to: " << filename.str());
        }
    }

    // This program performs calculations and file I/O directly, without scheduling
    // simulation events. Thus, Simulator::Run() and Simulator::Destroy() are not needed.

    return 0;
}