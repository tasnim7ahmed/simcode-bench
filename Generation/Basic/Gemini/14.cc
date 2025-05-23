#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

#include <iostream>
#include <map>
#include <vector>
#include <numeric> // For std::accumulate

using namespace ns3;

// Global variables for collecting statistics
// Map: Network ID -> AC (ns3::WifiQosMacType::MacTxQosCategory) -> Vector of TXOP durations
std::map<uint32_t, std::map<uint8_t, std::vector<double>>> g_txopDurations;
// Map: Network ID -> DSCP (as uint8_t, for app identification) -> Total received bytes
std::map<uint32_t, std::map<uint8_t, uint64_t>> g_receivedBytes;

// Trace callback for MacTxopStarted
void
MacTxopStartedTrace(uint32_t networkId, Ptr<const Packet> p, const Mac48Address& peerAddress,
                    WifiMacType macType, WifiQosMacType qosMacType, MacTxopReason reason,
                    Time duration)
{
    // Check if it's a QoS MAC and not a control/management packet (simulator internal data)
    // The duration parameter already represents the calculated TXOP duration in this trace.
    if (macType.IsQos() && !p->IsSimulatorData())
    {
        uint8_t ac = qosMacType.GetTxQosCategory(); // 0: BE, 1: BK, 2: VI, 3: VO
        g_txopDurations[networkId][ac].push_back(duration.GetSeconds());
    }
}

// Function to calculate and print statistics
void
PrintStats(uint32_t numNetworks, double simTime, bool enablePcap)
{
    std::cout << "\n--- Simulation Results ---" << std::endl;
    std::cout << "Simulation Time: " << simTime << "s" << std::endl;
    std::cout << "Pcap Enabled: " << (enablePcap ? "Yes" : "No") << std::endl;

    for (uint32_t i = 0; i < numNetworks; ++i)
    {
        std::cout << "\nNetwork " << i << ":" << std::endl;

        // Throughput
        std::cout << "  Throughput:" << std::endl;
        if (g_receivedBytes.count(i))
        {
            for (auto const& [dscp, bytes] : g_receivedBytes[i])
            {
                std::string ac_str;
                if (dscp == 0)
                {
                    ac_str = "AC_BE (DSCP 0)";
                }
                else if (dscp == 46)
                {
                    ac_str = "AC_VI (DSCP 46)";
                }
                else
                {
                    ac_str = "Unknown AC (DSCP " + std::to_string(dscp) + ")";
                }

                double throughputMbps = (bytes * 8.0) / (simTime * 1e6);
                std::cout << "    " << ac_str << ": " << bytes << " bytes received ("
                          << throughputMbps << " Mbps)" << std::endl;
            }
        }
        else
        {
            std::cout << "    No throughput data for this network." << std::endl;
        }

        // TXOP Duration
        std::cout << "  TXOP Durations:" << std::endl;
        if (g_txopDurations.count(i))
        {
            for (auto const& [ac, durations] : g_txopDurations[i])
            {
                std::string ac_name;
                if (ac == 0)
                {
                    ac_name = "AC_BE";
                }
                else if (ac == 1)
                {
                    ac_name = "AC_BK";
                }
                else if (ac == 2)
                {
                    ac_name = "AC_VI";
                }
                else if (ac == 3)
                {
                    ac_name = "AC_VO";
                }
                else
                {
                    ac_name = "Unknown AC (" + std::to_string(ac) + ")";
                }

                if (!durations.empty())
                {
                    double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
                    double avg = sum / durations.size();
                    std::cout << "    " << ac_name << ": Avg " << avg * 1000 << " ms (Total samples: "
                              << durations.size() << ")" << std::endl;
                }
                else
                {
                    std::cout << "    " << ac_name << ": No TXOP durations recorded." << std::endl;
                }
            }
        }
        else
        {
            std::cout << "    No TXOP duration data for this network." << std::endl;
        }
    }
    std::cout << "-------------------------" << std::endl;
}

int main(int argc, char* argv[])
{
    // 1. Configure default parameters
    uint32_t payloadSize = 1472;      // Bytes (max Ethernet payload is 1500, subtract IP/UDP headers)
    double distance = 10.0;           // Meters
    uint32_t rtsCtsThreshold = 2200;  // Bytes (disable RTS/CTS by default if > max MSDU size)
    double simTime = 10.0;            // Seconds
    bool enablePcap = false;
    uint32_t numNetworks = 4;
    std::string wifiStandard = "802.11n"; // Choose a standard that supports QoS well

    // 2. Command-line support
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size of application packets (bytes)", payloadSize);
    cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold (bytes), 0 to always use, >max_MSDU_size to disable", rtsCtsThreshold);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.AddValue("enablePcap", "Enable or disable pcap traces (true/false)", enablePcap);
    cmd.AddValue("numNetworks", "Number of independent Wi-Fi networks (max 4, min 1)", numNetworks);
    cmd.AddValue("wifiStandard", "Wi-Fi standard (e.g., 802.11a, 802.11b, 802.11g, 802.11n, 802.11ac)", wifiStandard);
    cmd.Parse(argc, argv);

    // Enforce numNetworks bounds
    if (numNetworks > 4)
    {
        numNetworks = 4;
    }
    if (numNetworks < 1)
    {
        numNetworks = 1;
    }

    // 3. Configure Wi-Fi parameters
    Config::SetDefault("ns3::OnOffApplication::PacketSize", UintegerValue(payloadSize));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("5Mbps"));

    // QoS DSCP settings: DSCP is in bits 2-7 of TOS field. Tos = DSCP << 2.
    // Default DSCP 0 (BE) maps to AC_BE.
    // DSCP 46 (0x2E, EF) maps to AC_VI by default in 802.11e.
    uint8_t dscpBe = 0;
    uint8_t dscpVi = 46;

    WifiHelper wifi;
    if (wifiStandard == "802.11a")
    {
        wifi.SetStandard(WIFI_PHY_STANDARD_80211a);
    }
    else if (wifiStandard == "802.11b")
    {
        wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    }
    else if (wifiStandard == "802.11g")
    {
        wifi.SetStandard(WIFI_PHY_STANDARD_80211g);
    }
    else if (wifiStandard == "802.11n")
    {
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n_5GHZ); // Using 5GHz band for n for simplicity
    }
    else if (wifiStandard == "802.11ac")
    {
        wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);
    }
    else
    {
        NS_FATAL_ERROR("Unknown Wi-Fi standard: " << wifiStandard);
    }

    YansWifiPhyHelper phy;
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Set RTS/CTS threshold
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsCtsThreshold));

    InternetStackHelper stack;
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    std::vector<NodeContainer> apNodes(numNetworks);
    std::vector<NodeContainer> staNodes(numNetworks);
    std::vector<NetDeviceContainer> apDevices(numNetworks);
    std::vector<NetDeviceContainer> staDevices(numNetworks);
    std::vector<Ipv4InterfaceContainer> apInterfaces(numNetworks);
    std::vector<Ipv4InterfaceContainer> staInterfaces(numNetworks);

    // Store application pointers to retrieve statistics later
    std::vector<Ptr<UdpServer>> serverApps(numNetworks * 2);
    std::vector<Ptr<OnOffApplication>> clientApps(numNetworks * 2);

    Ipv4AddressHelper address;
    uint32_t baseIp = 10;
    int channelNumbers[] = {1, 6, 11, 16}; // Non-overlapping 2.4GHz channels

    for (uint32_t i = 0; i < numNetworks; ++i)
    {
        // Create nodes
        apNodes[i].Create(1);
        staNodes[i].Create(1);

        // Install Internet stack
        stack.Install(apNodes[i]);
        stack.Install(staNodes[i]);

        // Position nodes
        Ptr<ConstantPositionMobilityModel> apMobility = CreateObject<ConstantPositionMobilityModel>();
        apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
        apNodes[i].Get(0)->AggregateObject(apMobility);

        Ptr<ConstantPositionMobilityModel> staMobility = CreateObject<ConstantPositionMobilityModel>();
        staMobility->SetPosition(Vector(distance, 0.0, 0.0));
        staNodes[i].Get(0)->AggregateObject(staMobility);

        // Configure Wi-Fi devices
        phy.Set("ChannelNumber", UintegerValue(channelNumbers[i]));

        QosWifiMacHelper mac; // Use QosWifiMacHelper for QoS support

        // AP MAC
        Ssid ssid = Ssid("ns3-wifi-" + std::to_string(i));
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid),
                                      "BeaconInterval", TimeValue(MicroSeconds(1024)));
        apDevices[i] = wifi.Install(phy, mac, apNodes[i]);

        // STA MAC
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid),
                                      "ActiveProbing", BooleanValue(false)); // STA passively scans for the specific SSID
        staDevices[i] = wifi.Install(phy, mac, staNodes[i]);

        // Assign IP addresses
        address.SetBase(Ipv4Address(baseIp + i, 0, 0, 0), Ipv4Mask("255.255.255.0"));
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);

        // Install applications (UdpServer on AP, OnOffApplication on STA)
        // Two applications per STA, one for BE and one for VI traffic, with different ports.
        uint16_t portBE = 9 + (i * 2);  // Unique port for BE traffic for current network
        uint16_t portVI = 10 + (i * 2); // Unique port for VI traffic for current network

        // Server for BE traffic
        UdpServerHelper serverHelperBE(portBE);
        ApplicationContainer serverAppBE = serverHelperBE.Install(apNodes[i]);
        serverAppBE.Start(Seconds(0.0));
        serverAppBE.Stop(Seconds(simTime));
        serverApps[i * 2] = DynamicCast<UdpServer>(serverAppBE.Get(0));

        // Server for VI traffic
        UdpServerHelper serverHelperVI(portVI);
        ApplicationContainer serverAppVI = serverHelperVI.Install(apNodes[i]);
        serverAppVI.Start(Seconds(0.0));
        serverAppVI.Stop(Seconds(simTime));
        serverApps[i * 2 + 1] = DynamicCast<UdpServer>(serverAppVI.Get(0));

        // Client for BE traffic
        OnOffHelper clientHelperBE("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces[i].GetAddress(0), portBE));
        clientHelperBE.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelperBE.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelperBE.SetAttribute("DataRate", StringValue("5Mbps")); // Example data rate for BE
        clientHelperBE.SetAttribute("PacketSize", UintegerValue(payloadSize));
        // Set DSCP for BE
        clientHelperBE.SetAttribute("EnableQos", BooleanValue(true)); // Necessary for QoS to interpret TOS/DSCP
        clientHelperBE.SetAttribute("Tos", UintegerValue(dscpBe << 2)); // DSCP is in bits 2-7 of TOS field
        ApplicationContainer clientAppBE = clientHelperBE.Install(staNodes[i]);
        clientAppBE.Start(Seconds(1.0));
        clientAppBE.Stop(Seconds(simTime - 1.0)); // Stop slightly before end to allow final packets to clear
        clientApps[i * 2] = DynamicCast<OnOffApplication>(clientAppBE.Get(0));

        // Client for VI traffic
        OnOffHelper clientHelperVI("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces[i].GetAddress(0), portVI));
        clientHelperVI.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelperVI.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelperVI.SetAttribute("DataRate", StringValue("10Mbps")); // Example data rate for VI (higher priority)
        clientHelperVI.SetAttribute("PacketSize", UintegerValue(payloadSize));
        // Set DSCP for VI
        clientHelperVI.SetAttribute("EnableQos", BooleanValue(true)); // Necessary for QoS
        clientHelperVI.SetAttribute("Tos", UintegerValue(dscpVi << 2)); // DSCP is in bits 2-7 of TOS field
        ApplicationContainer clientAppVI = clientHelperVI.Install(staNodes[i]);
        clientAppVI.Start(Seconds(1.0));
        clientAppVI.Stop(Seconds(simTime - 1.0));
        clientApps[i * 2 + 1] = DynamicCast<OnOffApplication>(clientAppVI.Get(0));

        // Enable pcap tracing
        if (enablePcap)
        {
            phy.EnablePcap("ap-" + std::to_string(i), apDevices[i]);
            phy.EnablePcap("sta-" + std::to_string(i), staDevices[i]);
        }

        // Connect trace sources for TXOP duration on STA's MAC layer
        // MacTxopStarted provides the duration directly for the completed TXOP
        Ptr<QosWifiMac> staQosMac = DynamicCast<QosWifiMac>(staDevices[i].Get(0)->GetMac());
        if (staQosMac)
        {
            staQosMac->TraceConnectWithoutContext("MacTxopStarted", MakeBoundCallback(&MacTxopStartedTrace, i));
        }
    }

    // 4. Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // 5. Collect throughput data from UdpServer applications
    for (uint32_t i = 0; i < numNetworks; ++i)
    {
        // For BE traffic
        if (serverApps[i * 2])
        {
            uint64_t receivedBytesBE = serverApps[i * 2]->GetTotalReceivedBytes();
            g_receivedBytes[i][dscpBe] = receivedBytesBE;
        }
        // For VI traffic
        if (serverApps[i * 2 + 1])
        {
            uint64_t receivedBytesVI = serverApps[i * 2 + 1]->GetTotalReceivedBytes();
            g_receivedBytes[i][dscpVi] = receivedBytesVI;
        }
    }

    // 6. Print results
    PrintStats(numNetworks, simTime, enablePcap);

    Simulator::Destroy();
    return 0;
}