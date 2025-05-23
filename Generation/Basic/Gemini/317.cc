#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

NS_LOG_COMPONENT_DEFINE ("LteUdpSingleUeEnb");

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    LogComponentEnable ("LteUdpSingleUeEnb", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<EpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    NodeContainer enbNodes;
    enbNodes.Create (1);
    NodeContainer ueNodes;
    ueNodes.Create (1);

    Ptr<MobilityHelper> mobility = CreateObject<MobilityHelper> ();
    mobility->SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility->Install (enbNodes);
    mobility->Install (ueNodes);

    enbNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (0, 0, 0));
    ueNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (50, 0, 0));

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    Ptr<InternetStackHelper> internet = CreateObject<InternetStackHelper> ();
    internet->Install (enbNodes);
    internet->Install (ueNodes);

    lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address (ueLteDevs);
    // Ipv4Address ueAddress = ueIpIface.GetAddress (0); // UE's IP address

    Ptr<Node> pgw = epcHelper->GetPgw ();
    Ptr<PointToPointHelper> p2pHelper = CreateObject<PointToPointHelper> ();
    p2pHelper->SetDeviceAttribute ("DataRate", StringValue ("10Gbps"));
    p2pHelper->SetChannelAttribute ("Delay", StringValue ("1ms"));
    NetDeviceContainer internetDevices = p2pHelper->Install (pgw, enbNodes.Get(0));

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);

    Ipv4Address enbServerAddress = internetIpIfaces.GetAddress(1); 

    internet->PopulateRoutingTables ();

    uint16_t port = 9;

    Ptr<UdpServerHelper> server = CreateObject<UdpServerHelper> (port);
    ApplicationContainer serverApps = server->Install (enbNodes.Get(0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    uint32_t maxPackets = 1000;
    Time packetInterval = Seconds (0.01);
    uint32_t packetSize = 1024;

    Ptr<UdpClientHelper> client = CreateObject<UdpClientHelper> (enbServerAddress, port);
    client->SetAttribute ("MaxPackets", UintegerValue (maxPackets));
    client->SetAttribute ("Interval", TimeValue (packetInterval));
    client->SetAttribute ("PacketSize", UintegerValue (packetSize));
    ApplicationContainer clientApps = client->Install (ueNodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));

    Simulator::Run ();

    Simulator::Destroy ();

    return 0;
}