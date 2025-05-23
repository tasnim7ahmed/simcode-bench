#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

NS_LOG_COMPONENT_DEFINE ("WifiUdpMultipleClients");

int main (int argc, char *argv[])
{
    uint32_t numClients = 3;
    CommandLine cmd (__FILE__);
    cmd.AddValue ("numClients", "Number of client nodes", numClients);
    cmd.Parse (argc, argv);

    LogComponentEnable ("WifiUdpMultipleClients", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);

    NodeContainer apNode;
    apNode.Create (1);
    NodeContainer clientNodes;
    clientNodes.Create (numClients);

    NodeContainer allNodes;
    allNodes.Add (apNode);
    allNodes.Add (clientNodes);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n);

    YansWifiPhyHelper phy;
    phy.SetChannel (YansWifiChannelHelper::Create ());

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-wifi");

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid),
                 "BeaconInterval", TimeValue (NanoSeconds (102400)));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, apNode.Get (0));

    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid));

    NetDeviceContainer clientDevices;
    clientDevices = wifi.Install (phy, mac, clientNodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (allNodes);

    InternetStackHelper stack;
    stack.Install (allNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apIpInterface;
    apIpInterface = address.Assign (apDevice);

    Ipv4InterfaceContainer clientIpInterfaces;
    clientIpInterfaces = address.Assign (clientDevices);

    Ipv4Address serverAddress = apIpInterface.GetAddress (0);

    uint16_t port = 9;

    UdpEchoServerHelper echoServer (port);
    ApplicationContainer serverApps = echoServer.Install (apNode.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpEchoClientHelper echoClient (serverAddress, port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = echoClient.Install (clientNodes);
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (9.0));

    Simulator::Stop (Seconds (10.0));

    phy.EnablePcapAll ("wifi-udp-multiple-clients");

    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}