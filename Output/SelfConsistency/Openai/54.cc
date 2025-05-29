/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211acTest");

std::string McsToString(uint8_t mcs)
{
    std::ostringstream oss;
    oss << "MCS" << unsigned(mcs);
    return oss.str();
}

int
main (int argc, char *argv[])
{
    // Simulation parameters (with default values)
    bool useTcp = false;
    double distance = 5.0;
    double simulationTime = 10.0; // seconds
    uint32_t payloadSize = 1472; // bytes
    bool verbose = false;
    std::string dataRate = "1Gbps"; // Application data rate

    CommandLine cmd(__FILE__);
    cmd.AddValue ("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.AddValue ("distance", "Distance (meters) between STA and AP", distance);
    cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue ("verbose", "Enable verbose logging", verbose);
    cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue ("dataRate", "Application data rate", dataRate);
    cmd.Parse (argc, argv);

    if (verbose)
    {
        LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);
        LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
    }

    // Parameter sets
    std::vector<uint8_t> mcsList = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<uint16_t> channelWidths = {20, 40, 80, 160};
    std::vector<std::string> guardIntervals = {"SHORT", "LONG"}; // Short = 400ns, Long = 800ns
    std::vector<bool> rtsCtsList = {false, true};

    uint32_t runNumber = 0;
    for (auto chWidth : channelWidths)
    {
        for (auto gi : guardIntervals)
        {
            for (auto mcs : mcsList)
            {
                for (auto rtsCts : rtsCtsList)
                {
                    // Set run number for RNG stream separation
                    RngSeedManager::SetRun (runNumber++);

                    // 1. Create nodes
                    NodeContainer wifiStaNode;
                    wifiStaNode.Create (1);
                    NodeContainer wifiApNode;
                    wifiApNode.Create (1);

                    // 2. Set up Wi-Fi PHY and MAC with 802.11ac
                    SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default ();
                    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
                    phy.SetChannel (channel.Create ());

                    SpectrumWifiHelper wifi;
                    wifi.SetStandard (WIFI_STANDARD_80211ac);

                    uint8_t centerFreqIdx;
                    if (chWidth == 20)
                        centerFreqIdx = 42;
                    else if (chWidth == 40)
                        centerFreqIdx = 38;
                    else if (chWidth == 80)
                        centerFreqIdx = 42;
                    else // 160
                        centerFreqIdx = 50;

                    phy.Set ("ChannelNumber", UintegerValue (36)); // Control channel, match to centerFreqIdx if needed
                    phy.Set ("ChannelWidth", UintegerValue (chWidth));
                    // 80+80 MHz not well supported in all workflows; mapping to 160 if not possible

                    // Guard Interval
                    if (gi == "SHORT")
                        wifi.Set ("ShortGuardEnabled", BooleanValue (true));
                    else
                        wifi.Set ("ShortGuardEnabled", BooleanValue (false));

                    // RTS/CTS threshold (bytes)
                    uint32_t rtsCtsThresh = rtsCts ? 0 : 999999;
                    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (rtsCtsThresh));

                    // AP and STA helpers
                    Ssid ssid = Ssid ("wifi-ac");

                    WifiMacHelper mac;
                    NetDeviceContainer staDevice, apDevice;

                    // MCS: Disable rate control; fix data mode/MCS on both sides
                    std::ostringstream mcsStr;
                    mcsStr << "VhtMcs" << unsigned(mcs);

                    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                                  "DataMode", StringValue (mcsStr.str ()),
                                                  "ControlMode", StringValue (mcsStr.str ()));

                    // STA
                    mac.SetType ("ns3::StaWifiMac",
                                 "Ssid", SsidValue (ssid),
                                 "ActiveProbing", BooleanValue (false));
                    staDevice = wifi.Install (phy, mac, wifiStaNode);

                    // AP
                    mac.SetType ("ns3::ApWifiMac",
                                 "Ssid", SsidValue (ssid));
                    apDevice = wifi.Install (phy, mac, wifiApNode);

                    // 3. Mobility
                    MobilityHelper mobility;
                    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
                    positionAlloc->Add (Vector (0.0, 0.0, 0.0)); // AP
                    positionAlloc->Add (Vector (distance, 0.0, 0.0)); // STA
                    mobility.SetPositionAllocator (positionAlloc);
                    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
                    mobility.Install (wifiApNode);
                    mobility.Install (wifiStaNode);

                    // 4. Internet stack
                    InternetStackHelper stack;
                    stack.Install (wifiApNode);
                    stack.Install (wifiStaNode);

                    Ipv4AddressHelper address;
                    address.SetBase ("192.168.1.0", "255.255.255.0");
                    Ipv4InterfaceContainer staInterfaces, apInterfaces;
                    staInterfaces = address.Assign (staDevice);
                    apInterfaces = address.Assign (apDevice);

                    // 5. Application: UDP or TCP
                    uint16_t port = 9;
                    ApplicationContainer serverApp;
                    ApplicationContainer clientApp;

                    if (!useTcp)
                    {
                        // UDP
                        UdpServerHelper server (port);
                        serverApp = server.Install (wifiApNode.Get (0));
                        serverApp.Start (Seconds (0.0));
                        serverApp.Stop (Seconds (simulationTime + 1.0));

                        UdpClientHelper client (apInterfaces.GetAddress (0), port);
                        client.SetAttribute ("MaxPackets", UintegerValue (100000000));
                        client.SetAttribute ("Interval", TimeValue (Seconds (0.0)));
                        client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                        clientApp = client.Install (wifiStaNode.Get (0));
                        clientApp.Start (Seconds (1.0));
                        clientApp.Stop (Seconds (simulationTime + 1.0));
                    }
                    else
                    {
                        // TCP
                        PacketSinkHelper sink ("ns3::TcpSocketFactory",
                                              InetSocketAddress (Ipv4Address::GetAny (), port));
                        serverApp = sink.Install (wifiApNode.Get (0));
                        serverApp.Start (Seconds (0.0));
                        serverApp.Stop (Seconds (simulationTime + 1.0));

                        OnOffHelper client ("ns3::TcpSocketFactory",
                                            InetSocketAddress (apInterfaces.GetAddress (0), port));
                        client.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
                        client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                        client.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
                        client.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime + 1.0)));
                        clientApp = client.Install (wifiStaNode.Get (0));
                    }

                    // 6. FlowMonitor for throughput (goodput) calculation
                    FlowMonitorHelper flowmon;
                    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

                    Simulator::Stop (Seconds (simulationTime + 1.0));
                    Simulator::Run ();

                    // 7. Results: Get throughput/goodput from FlowMonitor stats
                    monitor->CheckForLostPackets ();
                    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier ());
                    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

                    double goodput = 0;
                    for (auto const& pair : stats)
                    {
                        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (pair.first);
                        // Downlink: STA (src) -> AP (dst)
                        if ((t.destinationAddress == apInterfaces.GetAddress(0)) &&
                            (t.sourceAddress == staInterfaces.GetAddress(0)))
                        {
                            double throughput =
                                (pair.second.rxBytes * 8.0) /
                                (simulationTime * 1e6); // Mbit/s
                            goodput = throughput;
                        }
                    }

                    // 8. Output line (CSV-style)
                    std::cout << "Protocol," << (useTcp ? "TCP" : "UDP")
                              << ",MCS," << unsigned(mcs)
                              << ",ChannelWidth," << chWidth
                              << ",GuardInterval," << gi
                              << ",RTS_CTS," << (rtsCts ? "ON" : "OFF")
                              << ",Distance," << distance
                              << ",Goodput_Mbps," << goodput
                              << std::endl;

                    Simulator::Destroy ();
                }
            }
        }
    }

    return 0;
}