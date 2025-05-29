#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DsdvVANETSimulation");

int main (int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    double distance = 50.0;
    double speed = 20.0;
    double simTime = 60.0;
    double txPower = 20.0;
    uint16_t port = 5000;

    CommandLine cmd;
    cmd.AddValue ("numVehicles", "Number of vehicles", numVehicles);
    cmd.AddValue ("distance", "Distance between vehicles (m)", distance);
    cmd.AddValue ("speed", "Vehicle speed (m/s)", speed);
    cmd.AddValue ("simTime", "Simulation time (s)", simTime);
    cmd.Parse (argc, argv);

    NodeContainer vehicles;
    vehicles.Create (numVehicles);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211p);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (txPower));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (txPower));

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default ();
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, vehicles);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
    for (uint32_t i = 0; i < numVehicles; ++i) {
        posAlloc->Add (Vector (i * distance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator (posAlloc);
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install (vehicles);
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel> ();
        mob->SetVelocity (Vector (speed, 0.0, 0.0));
    }

    InternetStackHelper stack;
    DsdvHelper dsdv;
    stack.SetRoutingHelper (dsdv);
    stack.Install (vehicles);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    uint32_t packetSize = 512;
    uint32_t numPackets = 1000;
    double interPacketInterval = 0.2;

    ApplicationContainer sinkApps, sourceApps;

    // Install a UDP sink on the last vehicle
    uint16_t sinkPort = port;
    Address sinkAddress (InetSocketAddress (interfaces.GetAddress (numVehicles-1), sinkPort));
    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
    sinkApps.Add (sinkHelper.Install (vehicles.Get (numVehicles-1)));

    // Install a UDP OnOffApplication on the first vehicle
    OnOffHelper onoff ("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute ("DataRate", StringValue ("2Mbps"));
    onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
    onoff.SetAttribute ("MaxBytes", UintegerValue (0));
    onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));
    onoff.SetAttribute ("StopTime", TimeValue (Seconds (simTime - 1.0)));
    sourceApps.Add (onoff.Install (vehicles.Get (0)));

    sinkApps.Start (Seconds (0.5));
    sinkApps.Stop (Seconds (simTime));
    sourceApps.Stop (Seconds (simTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    uint32_t receivedPackets = 0;
    uint32_t transmittedPackets = 0;
    uint64_t receivedBytes = 0;

    for (auto it = stats.begin (); it != stats.end (); ++it)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
        if (t.destinationPort == sinkPort)
        {
            transmittedPackets += it->second.txPackets;
            receivedPackets += it->second.rxPackets;
            receivedBytes += it->second.rxBytes;
        }
    }

    std::cout << "Total Transmitted Packets: " << transmittedPackets << std::endl;
    std::cout << "Total Received Packets:    " << receivedPackets << std::endl;
    std::cout << "Packet Delivery Ratio:     "
              << (transmittedPackets ? 100.0 * receivedPackets / transmittedPackets : 0.0) << "%" << std::endl;
    std::cout << "Received Bytes:            " << receivedBytes << std::endl;

    Simulator::Destroy ();
    return 0;
}