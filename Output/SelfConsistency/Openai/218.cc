#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VanetDsdvExample");

int main (int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    double simTime = 60.0; // seconds
    double distance = 50.0; // meters between vehicles
    double nodeSpeed = 20.0; // m/s

    CommandLine cmd;
    cmd.AddValue ("numVehicles", "Number of vehicles (nodes)", numVehicles);
    cmd.AddValue ("distance", "Distance between vehicles (m)", distance);
    cmd.AddValue ("nodeSpeed", "Speed of vehicles (m/s)", nodeSpeed);
    cmd.AddValue ("simTime", "Simulation time (s)", simTime);
    cmd.Parse (argc, argv);

    // 1. Create nodes
    NodeContainer vehicles;
    vehicles.Create (numVehicles);

    // 2. Set up mobility: nodes spaced along x axis, move at constant speed
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        positionAlloc->Add (Vector (i * distance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
    mobility.Install (vehicles);
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> cvm = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
        cvm->SetVelocity (Vector (nodeSpeed, 0.0, 0.0));
    }
    
    // 3. Set up WiFi (802.11p - WAVE is common for VANETs, but we'll use 802.11b/g for simplicity)
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    mac.SetType ("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (phy, mac, vehicles);

    // 4. Internet stack and DSDV
    DsdvHelper dsdv;
    InternetStackHelper internet;
    internet.SetRoutingHelper (dsdv);
    internet.Install (vehicles);

    // 5. Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // 6. Install UDP echo applications: first vehicle sends to last vehicle
    uint16_t port = 9;
    UdpEchoServerHelper echoServer (port);
    ApplicationContainer serverApps = echoServer.Install (vehicles.Get (numVehicles - 1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (simTime - 1));

    UdpEchoClientHelper echoClient (interfaces.GetAddress (numVehicles - 1), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (512));

    ApplicationContainer clientApps = echoClient.Install (vehicles.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (simTime - 1));

    // 7. FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

    // 8. Enable PCAP tracing if desired
    phy.EnablePcapAll ("vanet-dsdv");

    // 9. Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();

    // 10. Report results
    flowmon->CheckForLostPackets ();

    // Print summary statistics
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats ();
    uint64_t txPacketsSum = 0;
    uint64_t rxPacketsSum = 0;
    double rxBytes = 0.0;
    double delaySum = 0.0;

    for (auto iter = stats.begin (); iter != stats.end (); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
        if (t.destinationPort == port)
        {
            txPacketsSum += iter->second.txPackets;
            rxPacketsSum += iter->second.rxPackets;
            rxBytes += iter->second.rxBytes;
            delaySum += iter->second.delaySum.GetSeconds ();
        }
    }

    std::cout << "Simulation Results for VANET with DSDV\n";
    std::cout << "--------------------------------------\n";
    std::cout << "Total Packets Sent: " << txPacketsSum << "\n";
    std::cout << "Total Packets Received: " << rxPacketsSum << "\n";
    std::cout << "Packet Delivery Ratio: " 
              << (txPacketsSum > 0 ? static_cast<double> (rxPacketsSum) / txPacketsSum * 100.0 : 0)
              << "%\n";
    std::cout << "Total Received Bytes: " << rxBytes << "\n";
    std::cout << "Average End-to-End Delay: " 
              << (rxPacketsSum > 0 ? delaySum / rxPacketsSum : 0)
              << " seconds\n";

    Simulator::Destroy ();
    return 0;
}