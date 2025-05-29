#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetDsdvSimulation");

int main (int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    double simTime = 60.0; // seconds
    double distance = 30.0; // meters between vehicles
    double nodeSpeed = 15.0; // m/s (54km/h)
    uint32_t packetSize = 512; // bytes
    uint32_t numPackets = 200;
    double interval = 0.2; // seconds

    CommandLine cmd;
    cmd.AddValue ("numVehicles", "Number of vehicle nodes", numVehicles);
    cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue ("distance", "Distance between vehicles (meters)", distance);
    cmd.AddValue ("nodeSpeed", "Vehicle speed (m/s)", nodeSpeed);
    cmd.Parse (argc, argv);

    NodeContainer vehicles;
    vehicles.Create (numVehicles);

    // Wifi PHY/MAC
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
    WifiMacHelper wifiMac;

    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue ("ErpOfdmRate6Mbps"),
                                 "ControlMode", StringValue ("ErpOfdmRate6Mbps"));

    wifiMac.SetType ("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, vehicles);

    // Mobility model: straight road, constant speed, random start positions
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (distance),
                                  "DeltaY", DoubleValue (0.0),
                                  "GridWidth", UintegerValue (numVehicles),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install (vehicles);

    for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
        Ptr<ConstantVelocityMobilityModel> m = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
        m->SetVelocity (Vector (nodeSpeed, 0.0, 0.0));
    }

    // Install Internet stack with DSDV
    DsdvHelper dsdv;
    InternetStackHelper stack;
    stack.SetRoutingHelper (dsdv);
    stack.Install (vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Install UDP application
    uint16_t port = 4000;
    ApplicationContainer apps;
    for (uint32_t i = 0; i < numVehicles - 1; ++i)
    {
        // Sender: node i, receiver: node i+1
        OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (i+1), port)));
        onoff.SetAttribute ("DataRate", StringValue ("6Mbps"));
        onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
        onoff.SetAttribute ("MaxBytes", UintegerValue (packetSize*numPackets));
        onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        apps.Add (onoff.Install (vehicles.Get (i)));

        PacketSinkHelper sink ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
        apps.Add (sink.Install (vehicles.Get (i+1)));
    }
    apps.Start (Seconds (2.0));
    apps.Stop (Seconds (simTime - 1));

    // Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable PCAP
    wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.EnablePcap ("vanet-dsdv", devices);

    // Logging mobility positions
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("vanet-dsdv-mobility.tr");

    for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
        Ptr<MobilityModel> mob = vehicles.Get (i)->GetObject<MobilityModel> ();
        Simulator::Schedule (Seconds (0.0), &MobilityModel::TraceConnectWithoutContext, mob, "CourseChange",
                             MakeBoundCallback ([] (Ptr<OutputStreamWrapper> out, std::string context, Ptr<const MobilityModel> m)
                             {
                                 Vector pos = m->GetPosition ();
                                 *out->GetStream () << Simulator::Now ().GetSeconds ()
                                                    << "\t" << m->GetObject<Node> ()->GetId ()
                                                    << "\t" << pos.x << "\t" << pos.y << "\t" << pos.z << std::endl;
                             }, stream));
    }

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // Print Flow Monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        if (flow.second.rxPackets > 0)
        {
            std::cout << "  Mean delay: " << (flow.second.delaySum.GetSeconds()/flow.second.rxPackets) << " s\n";
            std::cout << "  Mean jitter: " << (flow.second.jitterSum.GetSeconds()/flow.second.rxPackets) << " s\n";
        }
        std::cout << "  Throughput: " << flow.second.rxBytes * 8.0 / (simTime-2.0) / 1000 / 1000 << " Mbps\n";
    }

    Simulator::Destroy ();
    return 0;
}