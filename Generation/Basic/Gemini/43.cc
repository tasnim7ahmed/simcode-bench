#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/command-line.h"
#include <iostream>
#include <string>
#include <vector>

NS_LOG_COMPONENT_DEFINE("HiddenNode11nAggregation");

int main(int argc, char *argv[])
{
    // 1. Default parameters
    bool rtsCts = true;
    uint32_t ampduNumMsdu = 16; // Number of aggregated MPDUs
    uint32_t payloadSize = 1472; // Bytes (Max UDP payload for 1500 byte MTU)
    double simTrafficDuration = 9.0; // Seconds, actual duration of traffic
    double expectedThroughputMbps = 20.0; // Mbps

    // 2. Command line arguments
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("rtsCts", "Enable/disable RTS/CTS (true/false)", rtsCts);
    cmd.AddValue("ampduNumMsdu", "Number of aggregated MPDUs within an A-MPDU frame", ampduNumMsdu);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payloadSize);
    cmd.AddValue("simTrafficDuration", "Duration of traffic generation in seconds", simTrafficDuration);
    cmd.AddValue("expectedThroughput", "Expected total throughput in Mbps for verification", expectedThroughputMbps);
    cmd.Parse(argc, argv);

    // Sanity checks for input
    if (payloadSize > 1472) {
        NS_FATAL_ERROR("Payload size too large. Max is 1472 bytes for 1500 byte MTU (IP/UDP overhead).");
    }
    if (ampduNumMsdu == 0) {
        NS_FATAL_ERROR("ampduNumMsdu must be greater than 0.");
    }
    if (simTrafficDuration <= 0) {
        NS_FATAL_ERROR("simTrafficDuration must be positive.");
    }

    // 3. Configure logging
    ns3::LogComponentEnable("HiddenNode11nAggregation", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);

    // 4. Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(3); // AP (0), STA1 (1), STA2 (2)
    ns3::Ptr<ns3::Node> apNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> sta1Node = nodes.Get(1);
    ns3::Ptr<ns3::Node> sta2Node = nodes.Get(2);

    // 5. Configure mobility
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Manually set positions for hidden node scenario
    // AP at (0,0,0)
    ns3::Ptr<ns3::ConstantPositionMobilityModel> apMobility = apNode->GetObject<ns3::ConstantPositionMobilityModel>();
    apMobility->SetPosition(ns3::Vector(0.0, 0.0, 0.0));

    // STA1 at (3,0,0)
    ns3::Ptr<ns3::ConstantPositionMobilityModel> sta1Mobility = sta1Node->GetObject<ns3::ConstantPositionMobilityModel>();
    sta1Mobility->SetPosition(ns3::Vector(3.0, 0.0, 0.0));

    // STA2 at (-3,0,0)
    ns3::Ptr<ns3::ConstantPositionMobilityModel> sta2Mobility = sta2Node->GetObject<ns3::ConstantPositionMobilityModel>();
    sta2Mobility->SetPosition(ns3::Vector(-3.0, 0.0, 0.0));

    // Check distances:
    // STA1-AP: 3m (within 5m)
    // STA2-AP: 3m (within 5m)
    // STA1-STA2: 6m (outside 5m, thus hidden)

    // 6. Configure Wi-Fi
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n_5GHZ); // 802.11n, 5GHz band
    
    // Use ConstantRateWifiManager for simplicity and controlled rate
    // HtMcs7 offers 65 Mbps (20MHz, 1 spatial stream, no GI)
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue("HtMcs7"),
                                 "ControlMode", ns3::StringValue("HtMcs0"));

    // Configure Wi-Fi Channel and Propagation Loss
    ns3::YansWifiChannelHelper channel;
    // Set RangePropagationLossModel to 5 meters to create hidden node scenario
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "Range", ns3::DoubleValue(5.0));
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create();

    // Configure Wi-Fi Phy
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    // Setting typical WifiPhy attributes
    phy.Set("TxPowerStart", ns3::DoubleValue(10.0)); // dBm
    phy.Set("TxPowerEnd", ns3::DoubleValue(10.0)); // dBm
    phy.Set("RxSensitivity", ns3::DoubleValue(-90.0)); // dBm
    phy.Set("CcaMode1Threshold", ns3::DoubleValue(-85.0)); // dBm

    // Configure MAC layer (AP and STA)
    ns3::NqosWifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid("hidden-11n-network");

    // Configure AP MAC
    mac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid));
    ns3::NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Configure STA MACs
    mac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid), "ActiveProbing", ns3::BooleanValue(false));
    ns3::NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, ns3::NodeContainer(sta1Node, sta2Node));

    // Enable MPDU Aggregation (A-MPDU)
    ns3::Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtWifiMac/Dot11nMac/AmpduAggregation", ns3::BooleanValue(true));
    ns3::Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtWifiMac/Dot11nMac/Dot11nMpduAggNumBuffers", ns3::UintegerValue(ampduNumMsdu));
    NS_LOG_INFO("A-MPDU aggregation enabled with " << ampduNumMsdu << " MPDUs.");

    // Configure RTS/CTS threshold
    if (rtsCts) {
        // Set RTS/CTS threshold to 0 to enable for all unicast packets
        ns3::Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", ns3::UintegerValue(0));
        NS_LOG_INFO("RTS/CTS is ENABLED (threshold 0).");
    } else {
        // Set RTS/CTS threshold to a very large value to disable it
        ns3::Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/RtsCtsThreshold", ns3::UintegerValue(999999));
        NS_LOG_INFO("RTS/CTS is DISABLED (threshold 999999).");
    }

    // 7. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // 8. Setup UDP Traffic (Saturated flow from STAs to AP)
    uint16_t port = 9; // Discard port

    // Packet Sink (AP side)
    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer sinkApps = sinkHelper.Install(apNode);
    sinkApps.Start(ns3::Seconds(0.0));
    sinkApps.Stop(ns3::Seconds(simTrafficDuration + 2.0)); // Run slightly longer than traffic

    // OnOff Applications (STA1 and STA2 as clients)
    ns3::OnOffHelper clientHelper("ns3::UdpSocketFactory", ns3::InetSocketAddress(apInterface.GetAddress(0), port));
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(payloadSize));
    clientHelper.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    // Set a very high data rate to ensure saturated flow (e.g., 1000Mbps)
    clientHelper.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate("1000Mbps"))); 

    double appStart = 1.0; // Start traffic after 1 second
    ns3::ApplicationContainer clientAppsSta1 = clientHelper.Install(sta1Node);
    clientAppsSta1.Start(ns3::Seconds(appStart));
    clientAppsSta1.Stop(ns3::Seconds(appStart + simTrafficDuration));

    ns3::ApplicationContainer clientAppsSta2 = clientHelper.Install(sta2Node);
    clientAppsSta2.Start(ns3::Seconds(appStart));
    clientAppsSta2.Stop(ns3::Seconds(appStart + simTrafficDuration));

    // 9. Enable Tracing
    phy.EnablePcap("hidden-node-11n-ap", apDevice.Get(0));
    phy.EnablePcap("hidden-node-11n-sta1", staDevices.Get(0));
    phy.EnablePcap("hidden-node-11n-sta2", staDevices.Get(1));

    ns3::AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("hidden-node-11n-phy.tr"));
    mac.EnableAsciiAll(ascii.CreateFileStream("hidden-node-11n-mac.tr"));

    // 10. Simulation start and stop
    NS_LOG_INFO("Starting simulation for " << (appStart + simTrafficDuration + 1.0) << " seconds (Traffic duration: " << simTrafficDuration << "s)...");
    ns3::Simulator::Stop(ns3::Seconds(appStart + simTrafficDuration + 1.0)); // Allow time for last packets to be processed
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();
    NS_LOG_INFO("Simulation finished.");

    // 11. Calculate throughput
    ns3::Ptr<ns3::PacketSink> sink = ns3::DynamicCast<ns3::PacketSink>(sinkApps.Get(0));
    double totalRxBytes = sink->GetTotalRxBytes();
    double actualThroughputMbps = (totalRxBytes * 8.0) / (simTrafficDuration * 1000000.0);

    NS_LOG_INFO("Total received bytes at AP: " << totalRxBytes << " bytes");
    NS_LOG_INFO("Actual aggregate throughput (over " << simTrafficDuration << "s): " << actualThroughputMbps << " Mbps");

    // 12. Verify throughput
    double tolerance = 0.10; // 10% tolerance
    double lowerBound = expectedThroughputMbps * (1.0 - tolerance);
    double upperBound = expectedThroughputMbps * (1.0 + tolerance);

    if (actualThroughputMbps >= lowerBound && actualThroughputMbps <= upperBound)
    {
        NS_LOG_INFO("Throughput verification PASSED. Expected: " << expectedThroughputMbps << " Mbps, Actual: " << actualThroughputMbps << " Mbps (within " << (tolerance*100) << "% tolerance).");
    }
    else
    {
        NS_LOG_WARN("Throughput verification FAILED. Expected: " << expectedThroughputMbps << " Mbps, Actual: " << actualThroughputMbps << " Mbps (outside " << (tolerance*100) << "% tolerance).");
        // return 1; // Uncomment to indicate failure in automated test systems
    }

    return 0;
}