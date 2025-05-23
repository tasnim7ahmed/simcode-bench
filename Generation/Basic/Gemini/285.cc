#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"

NS_LOG_COMPONENT_DEFINE ("LteUdpMobilitySimulation");

int main (int argc, char *argv[])
{
    // Define simulation parameters
    uint32_t numUe = 3;
    double simTime = 10.0;
    uint16_t udpPort = 5000;
    uint32_t packetSize = 512; // bytes
    // 512 bytes / 10 ms = 512 bytes / 0.01 s = 51200 bytes/s = 51.2 kbps
    std::string dataRate = "51200bps";

    // 1. Create LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    // 2. Create nodes
    NodeContainer enbNodes;
    enbNodes.Create (1);
    NodeContainer ueNodes;
    ueNodes.Create (numUe);

    // 3. Install Mobility Models
    // eNodeB stationary at the center of the 100x100 area
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    enbPositionAlloc->Add (Vector (50.0, 50.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetPositionAllocator (enbPositionAlloc);
    enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    enbMobility.Install (enbNodes);

    // UE nodes mobile within a 100x100 area using RandomWalk2d
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator ("ns3::RandomRectanglePositionAllocator",
                                     "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                     "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                                  "Speed", StringValue ("ns3::UniformRandomVariable[Min=1.0|Max=2.0]"),
                                  "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.1]"));
    ueMobility.Install (ueNodes);

    // 4. Install Internet Stack on all nodes
    InternetStackHelper internet;
    internet.Install (enbNodes);
    internet.Install (ueNodes);

    // 5. Install LTE Devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    // 6. Attach UEs to eNodeB
    // Attach all UEs to the single eNodeB
    for (uint32_t i = 0; i < numUe; ++i)
    {
        lteHelper->Attach (ueLteDevs.Get (i), enbLteDevs.Get (0));
    }

    // 7. Configure Applications
    // UE 0 as UDP Server (Packet Sink)
    Ptr<Node> ue0 = ueNodes.Get (0);
    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                 InetSocketAddress (Ipv4Address::GetAny (), udpPort));
    ApplicationContainer serverApps = sinkHelper.Install (ue0);
    serverApps.Start (Seconds (0.5)); // Start server slightly after simulation start
    serverApps.Stop (Seconds (simTime));

    // Get UE 0's IP address for the client to send to
    Ptr<Ipv4> ipv4Ue0 = ue0->GetObject<Ipv4> ();
    // Find the LTE network device on UE0 and get its IP address
    int32_t interfaceIndexUe0 = -1;
    for (uint32_t i = 0; i < ipv4Ue0->GetNInterfaces (); ++i)
    {
        Ptr<NetDevice> device = ipv4Ue0->GetNetDevice (i);
        if (DynamicCast<LteUeNetDevice> (device))
        {
            interfaceIndexUe0 = ipv4Ue0->GetInterfaceForDevice (device);
            break;
        }
    }
    Ipv4Address ue0IpAddress = ipv4Ue0->GetAddress (interfaceIndexUe0, 0).GetLocal ();

    // UE 1 as UDP Client
    Ptr<Node> ue1 = ueNodes.Get (1);
    OnOffHelper clientHelper ("ns3::UdpSocketFactory",
                              InetSocketAddress (ue0IpAddress, udpPort));
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (packetSize));
    clientHelper.SetAttribute ("DataRate", DataRateValue (dataRate));
    ApplicationContainer clientApps = clientHelper.Install (ue1);
    clientApps.Start (Seconds (1.0)); // Start client after server
    clientApps.Stop (Seconds (simTime - 0.5)); // Stop client slightly before simulation end

    // 8. Enable PCAP Tracing
    lteHelper->EnablePcapAll ("lte-udp-mobility-sim");

    // 9. Run Simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}