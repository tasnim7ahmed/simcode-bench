#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::LteUePhy::HarqProcesses", UintegerValue(8));

    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(1);

    LteHelper lteHelper;
    lteHelper.SetEnbAntennaModelType ("ns3::CosineAntennaModel");
    lteHelper.SetEnbAntennaModelModelAttribute ("Orientation", DoubleValue (0));
    lteHelper.SetEnbAntennaModelModelAttribute ("Beamwidth", DoubleValue (60));
    lteHelper.SetEnbDeviceAttribute ("DlEarfcn", UintegerValue (3350));
    lteHelper.SetEnbDeviceAttribute ("UlEarfcn", UintegerValue (18300));

    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
    p2pHelper.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    p2pHelper.SetChannelAttribute ("Delay", StringValue ("0ms"));

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper.InstallEnbDevice (enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper.InstallUeDevice (ueNodes);

    InternetStackHelper internet;
    internet.Install (enbNodes);
    internet.Install (ueNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer enbIpIface = ipv4.Assign (enbDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign (ueDevs);

    lteHelper.Attach (ueDevs.Get (0), enbDevs.Get (0));

    uint16_t dlPort = 5000;
    Address dlSinkAddress (InetSocketAddress (ueIpIface.GetAddress (0), dlPort));
    PacketSinkHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", dlSinkAddress);
    ApplicationContainer dlSinkApps = dlPacketSinkHelper.Install (ueNodes.Get (0));
    dlSinkApps.Start (Seconds (0.0));
    dlSinkApps.Stop (Seconds (10.0));

    uint16_t ulPort = 5000;
    Address ulSinkAddress (InetSocketAddress (enbIpIface.GetAddress (0), ulPort));
    PacketSinkHelper ulPacketSinkHelper ("ns3::TcpSocketFactory", ulSinkAddress);
    ApplicationContainer ulSinkApps = ulPacketSinkHelper.Install (enbNodes.Get (0));
    ulSinkApps.Start (Seconds (0.0));
    ulSinkApps.Stop (Seconds (10.0));

    OnOffHelper onoff ("ns3::TcpSocketFactory", ulSinkAddress);
    onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute ("PacketSize", UintegerValue (1472));
    onoff.SetAttribute ("DataRate", StringValue ("10Mbps"));
    ApplicationContainer clientApps = onoff.Install (ueNodes.Get (0));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (10.0));

    Simulator::Stop (Seconds (10.0));

    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}