#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/pcap-file.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiRssFixedLossExample");

int main(int argc, char *argv[])
{
    double rss = -80.0;                // dBm
    uint32_t packetSize = 1024;        // bytes
    uint32_t numPackets = 10;
    double interval = 1.0;             // seconds
    bool verbose = false;

    CommandLine cmd;
    cmd.AddValue ("rss", "Received Signal Strength (dBm)", rss);
    cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue ("interval", "Interval between packets (seconds)", interval);
    cmd.AddValue ("verbose", "Enable verbose logging", verbose);
    cmd.Parse (argc, argv);

    if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("WifiRssFixedLossExample", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create (1);
    NodeContainer wifiApNode;
    wifiApNode.Create (1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    Ptr<FixedRssLossModel> lossModel = CreateObject<FixedRssLossModel> ();
    lossModel->SetRss (rss);
    channel.SetPropagationLossModel (lossModel);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());
    phy.Set("TxPowerStart", DoubleValue(20.0));
    phy.Set("TxPowerEnd", DoubleValue(20.0));
    phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns3-ssid");

    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install (phy, mac, wifiStaNode);

    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (5.0),
                                  "DeltaY", DoubleValue (10.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNode);
    mobility.Install (wifiApNode);

    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign (staDevice);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign (apDevice);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer (port);
    ApplicationContainer serverApps = echoServer.Install (wifiApNode.Get (0));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (20.0));

    UdpEchoClientHelper echoClient (apInterface.GetAddress (0), port);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (numPackets));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (interval)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (packetSize));

    ApplicationContainer clientApps = echoClient.Install (wifiStaNode.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (20.0));

    phy.EnablePcap ("wifi-rss-fixed-loss-ap", apDevice.Get (0));
    phy.EnablePcap ("wifi-rss-fixed-loss-sta", staDevice.Get (0));

    Simulator::Stop (Seconds (21.0));
    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}