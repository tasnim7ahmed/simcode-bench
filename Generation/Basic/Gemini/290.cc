#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

int main (int argc, char *argv[])
{
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper> ();
    lteHelper->SetEpcHelper (epcHelper);

    NodeContainer enbNodes;
    enbNodes.Create (1);
    NodeContainer ueNodes;
    ueNodes.Create (1);

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (enbNodes);
    enbNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (50, 50, 0));

    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (0, 100, 0, 100)),
                               "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install (ueNodes);

    InternetStackHelper internet;
    internet.Install (enbNodes);
    internet.Install (ueNodes);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

    lteHelper->AssignUeIps (ueLteDevs);

    lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

    Ipv4InterfaceContainer ueIpIfaces = lteHelper->GetUeIpInterfaces ();
    Ipv4Address ueIpAddress = ueIpIfaces.GetAddress (0);

    uint16_t serverPort = 5000;
    UdpServerHelper server (serverPort);
    ApplicationContainer serverApps = server.Install (ueNodes.Get (0));
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (10.0));

    UdpClientHelper client (ueIpAddress, serverPort);
    client.SetAttribute ("Interval", TimeValue (MilliSeconds (200)));
    client.SetAttribute ("PacketSize", UintegerValue (1024));
    client.SetAttribute ("MaxBytes", UintegerValue (0));

    ApplicationContainer clientApps = client.Install (enbNodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));

    lteHelper->EnablePhyTraces ();
    lteHelper->EnableMacTraces ();
    lteHelper->EnableRlcTraces ();

    Simulator::Stop (Seconds (10.0));

    Simulator::Run ();

    Simulator::Destroy ();

    return 0;
}