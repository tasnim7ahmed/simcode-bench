#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/spectrum-module.h"

int main (int argc, char *argv[])
{
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    NodeContainer enbNodes;
    enbNodes.Create (1);
    
    NodeContainer ueNodes;
    ueNodes.Create (1);

    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
    enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));
    MobilityHelper enbMobility;
    enbMobility.SetPositionAllocator (enbPositionAlloc);
    enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    enbMobility.Install (enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator (CreateObject<RandomRectanglePositionAllocator> (
                                         Rectangle (0, 100, 0, 100)));
    ueMobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                  "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                                  "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    ueMobility.Install (ueNodes);

    NetDeviceContainer enbLteDevices = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevices = lteHelper->InstallUeDevice (ueNodes);

    InternetStackHelper internet;
    internet.Install (ueNodes);
    internet.Install (enbNodes);

    epcHelper->CreateAndAttachEpcDevice (enbLteDevices.Get (0), enbNodes.Get (0));

    lteHelper->Attach (ueLteDevices.Get (0), enbLteDevices.Get (0));

    Ptr<Ipv4> enbIpv4 = enbNodes.Get (0)->GetObject<Ipv4> ();
    Ipv4Address enbServerAddress;
    Ptr<NetDevice> enbS1UDevice = epcHelper->GetS1UDeviceForEnb (enbNodes.Get (0));
    
    for (uint32_t i = 0; i < enbIpv4->GetNInterfaces(); ++i) {
        Ptr<Ipv4Interface> iface = enbIpv4->GetInterface(i);
        if (iface->GetDevice()->GetIfIndex() == enbS1UDevice->GetIfIndex()) {
            enbServerAddress = iface->GetAddress(0).GetLocal();
            break;
        }
    }

    uint16_t port = 9;
    PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                       InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer serverApps = packetSinkHelper.Install (enbNodes.Get (0));
    serverApps.Start (Seconds (0.5));
    serverApps.Stop (Seconds (9.5));

    UdpClientHelper udpClientHelper (enbServerAddress, port);
    udpClientHelper.SetAttribute ("MaxPackets", UintegerValue (5));
    udpClientHelper.SetAttribute ("Interval", TimeValue (Seconds (1)));
    udpClientHelper.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps = udpClientHelper.Install (ueNodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (6.0));

    Simulator::Stop (Seconds (10));

    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}