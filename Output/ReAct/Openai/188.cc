#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiStaToStaNetAnim");

int
main (int argc, char *argv[])
{
    uint32_t packetSize = 1024;
    uint32_t numPackets = 2000;
    double interval = 0.01;
    double simulationTime = 20.0; // seconds

    CommandLine cmd;
    cmd.Parse (argc, argv);

    // Create nodes: 2 STAs + 1 AP
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (2);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);

    // Wifi channel and PHY
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    // Wifi MAC and Helper
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi-ssid");

    // STA devices
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    // AP device
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));   // STA 0
    positionAlloc->Add (Vector (5.0, 0.0, 0.0));   // STA 1
    mobility.SetPositionAllocator (positionAlloc);

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodes);

    MobilityHelper mobilityAp;
    mobilityAp.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobilityAp.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (2.5),
                                    "MinY", DoubleValue (5.0),
                                    "DeltaX", DoubleValue (1.0),
                                    "DeltaY", DoubleValue (1.0),
                                    "GridWidth", UintegerValue (1),
                                    "LayoutType", StringValue ("RowFirst"));
    mobilityAp.Install (wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (wifiStaNodes);
    stack.Install (wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase ("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign (staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign (apDevice);

    // UDP server on STA1
    uint16_t port = 9;
    UdpServerHelper server (port);
    ApplicationContainer serverApp = server.Install (wifiStaNodes.Get(1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (simulationTime));

    // UDP client on STA0, sending to STA1
    UdpClientHelper client (staInterfaces.GetAddress(1), port);
    client.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    client.SetAttribute ("Interval", TimeValue (Seconds (interval)));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApp = client.Install (wifiStaNodes.Get(0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (simulationTime));

    // Enable NetAnim
    AnimationInterface anim ("wifi-sta-sta.xml");
    anim.SetConstantPosition (wifiStaNodes.Get(0), 0,0);
    anim.SetConstantPosition (wifiStaNodes.Get(1), 5,0);
    anim.SetConstantPosition (wifiApNode.Get(0), 2.5,5);

    // FlowMonitor for performance metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();

    // Throughput, packet loss
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
        if ((t.sourceAddress == staInterfaces.GetAddress(0) && t.destinationAddress == staInterfaces.GetAddress(1)) ||
            (t.sourceAddress == staInterfaces.GetAddress(1) && t.destinationAddress == staInterfaces.GetAddress(0)))
        {
            std::cout << "Flow " << iter->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << iter->second.txPackets << "\n";
            std::cout << "  Rx Packets: " << iter->second.rxPackets << "\n";
            std::cout << "  Lost Packets: " << iter->second.lostPackets << "\n";
            double throughput = iter->second.rxBytes * 8.0 / (simulationTime-2.0) / 1000; // Kbps
            std::cout << "  Throughput: " << throughput << " Kbps\n";
        }
    }

    Simulator::Destroy ();
    return 0;
}