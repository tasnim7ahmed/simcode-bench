#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include <fstream>
#include <string>
#include <vector>
#include <iomanip> // For std::fixed and std::setprecision

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiErrorRateValidation");

// Simulation parameters (configurable via command line)
uint32_t g_frameSize = 1500;    // Bytes (payload size of UDP packets)
uint32_t g_numPackets = 10000; // Number of packets to send for each SNR point
double g_minSnr = 0.0;          // dB
double g_maxSnr = 40.0;         // dB
double g_snrStep = 2.0;         // dB

int main(int argc, char *argv[])
{
    LogComponentEnable("WifiErrorRateValidation", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_WARN);
    LogComponentEnable("PacketSink", LOG_LEVEL_WARN);

    CommandLine cmd(__FILE__);
    cmd.AddValue("frameSize", "Size of the data frames in bytes (UDP payload)", g_frameSize);
    cmd.AddValue("numPackets", "Number of packets to send per SNR point", g_numPackets);
    cmd.AddValue("minSnr", "Minimum SNR in dB", g_minSnr);
    cmd.AddValue("maxSnr", "Maximum SNR in dB", g_maxSnr);
    cmd.AddValue("snrStep", "SNR step in dB", g_snrStep);
    cmd.Parse(argc, argv);

    // List of error models (PHY types) to test
    // Each of these PHY types implements its own error rate model.
    std::vector<std::string> phyTypes = {"ns3::NistWifiPhy", "ns3::YansWifiPhy", "ns3::TableWifiPhy"};
    
    // HE MCS values (0 to 11)
    std::vector<int> heMcsValues;
    for (int i = 0; i <= 11; ++i)
    {
        heMcsValues.push_back(i);
    }

    for (const auto &phyTypeName : phyTypes)
    {
        std::string modelName;
        if (phyTypeName == "ns3::NistWifiPhy") {
            modelName = "NIST";
        } else if (phyTypeName == "ns3::YansWifiPhy") {
            modelName = "YANS";
        } else if (phyTypeName == "ns3::TableWifiPhy") {
            modelName = "Table";
        } else {
            modelName = "Unknown"; // Should not happen
        }

        for (int mcs : heMcsValues)
        {
            // Create output file for this (errorModel, MCS) combination
            std::string filename = "fsr_" + modelName + "_HE_MCS" + std::to_string(mcs) + ".dat";
            std::ofstream outputFile(filename.c_str());
            if (!outputFile.is_open())
            {
                NS_LOG_ERROR("Could not open file: " << filename);
                return 1;
            }
            outputFile << "# SNR_dB\tFSR\n"; // Gnuplot header

            NS_LOG_INFO("Running simulation for Model: " << modelName << ", HE MCS: " << mcs);

            for (double snr_dB = g_minSnr; snr_dB <= g_maxSnr; snr_dB += g_snrStep)
            {
                // Clean up previous simulation state to ensure a fresh run
                Simulator::Destroy();
                Simulator::Reset();

                // Nodes
                NodeContainer nodes;
                nodes.Create(2);
                Ptr<Node> senderNode = nodes.Get(0);
                Ptr<Node> receiverNode = nodes.Get(1);

                // Mobility (static positions)
                MobilityHelper mobility;
                mobility.SetMobilityModelType("ns3::ConstantPositionMobilityModel");
                mobility.Install(nodes);
                // Place nodes close to each other, but distance is irrelevant due to ConstantSnrPropagationLossModel
                nodes.Get(0)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
                nodes.Get(1)->GetObject<ConstantPositionMobilityModel>()->SetPosition(Vector(1.0, 0.0, 0.0));

                // Wifi Channel setup
                // ConstantSnrPropagationLossModel directly sets the SNR at the receiver's PHY.
                Ptr<ConstantSnrPropagationLossModel> snrLoss = CreateObject<ConstantSnrPropagationLossModel>();
                snrLoss->SetSnr(snr_dB); // Set SNR in dB
                Ptr<ConstantPropagationDelayModel> delayModel = CreateObject<ConstantPropagationDelayModel>();
                Ptr<YansWifiChannel> channel = CreateObject<YansWifiChannel>();
                channel->SetPropagationLossModel(snrLoss);
                channel->SetPropagationDelayModel(delayModel);

                // Wifi PHY Helper and Error Model selection
                WifiPhyHelper phyHelper;
                phyHelper.SetChannel(channel);
                phyHelper.Set("PhyType", StringValue(phyTypeName)); // Set the specific PHY type (and thus error model)

                // Wifi MAC setup
                WifiMacHelper macHelper;
                macHelper.SetType("ns3::AdhocWifiMac"); // Simplest MAC type for direct communication

                // Install Wifi devices
                NetDeviceContainer devices;
                devices = WifiHelper::Default()->Install(phyHelper, macHelper, nodes);

                // Set HE MCS for both devices
                Ptr<WifiNetDevice> senderDevice = DynamicCast<WifiNetDevice>(devices.Get(0));
                Ptr<WifiNetDevice> receiverDevice = DynamicCast<WifiNetDevice>(devices.Get(1));

                Ptr<WifiPhy> senderPhy = senderDevice->GetPhy();
                Ptr<WifiPhy> receiverPhy = receiverDevice->GetPhy();

                // Set the desired HE MCS for both sender and receiver
                senderPhy->SetWifiMode(WifiModeFactory::CreateWifiMode(WIFI_STANDARD_80211ax, static_cast<WifiModes::HE_MCS>(mcs)));
                receiverPhy->SetWifiMode(WifiModeFactory::CreateWifiMode(WIFI_STANDARD_80211ax, static_cast<WifiModes::HE_MCS>(mcs)));
                
                // Disable A-MSDU and A-MPDU for simpler Frame Success Rate validation
                senderPhy->SetAttribute("ActiveMsduAggregation", BooleanValue(false));
                receiverPhy->SetAttribute("ActiveMsduAggregation", BooleanValue(false));
                senderPhy->SetAttribute("ActiveAmpduAggregation", BooleanValue(false));
                receiverPhy->SetAttribute("ActiveAmpduAggregation", BooleanValue(false));

                // Install Internet stack
                InternetStackHelper stack;
                stack.Install(nodes);

                Ipv4AddressHelper address;
                address.SetBase("10.1.1.0", "255.255.255.0");
                Ipv4InterfaceContainer interfaces = address.Assign(devices);

                // Application setup (OnOffApplication as sender, PacketSink as receiver)
                // OnOffApplication sends UDP packets
                OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]")); // Send continuously
                onoff.SetAttribute("PacketSize", UintegerValue(g_frameSize));
                onoff.SetAttribute("MaxBytes", UintegerValue(g_frameSize * g_numPackets)); // Total bytes to send
                onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps"))); // Ensure data rate is high enough to send all packets quickly

                ApplicationContainer senderApp = onoff.Install(senderNode);
                senderApp.Start(Seconds(0.1));
                senderApp.Stop(Seconds(10.0)); // A fixed duration for the app to run

                // PacketSink application to receive packets
                PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::Any, 9));
                ApplicationContainer receiverApp = sink.Install(receiverNode);
                receiverApp.Start(Seconds(0.0));
                receiverApp.Stop(Seconds(10.0));

                // Enable PHY stats for the receiver to count successful receptions
                WifiPhyStatsHelper phyStatsHelper;
                phyStatsHelper.EnablePhyRxStats(receiverPhy);

                // Run simulation
                Simulator::Stop(Seconds(10.0)); // Ensure simulation stops after applications
                Simulator::Run();

                // Collect results
                WifiPhyStats phyStats = phyStatsHelper.GetStats();
                uint64_t totalRxOk = phyStats.GetRxOk();
                
                // Calculate Frame Success Rate
                double fsr = (g_numPackets > 0) ? (double)totalRxOk / g_numPackets : 0.0;

                NS_LOG_INFO("  SNR: " << std::fixed << std::setprecision(1) << snr_dB << " dB, RxOk: " << totalRxOk << ", FSR: " << fsr);
                outputFile << std::fixed << std::setprecision(2) << snr_dB << "\t" << std::fixed << std::setprecision(6) << fsr << "\n";
            }
            outputFile.close();
        }
    }

    return 0;
}