#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiClearChannelExperiment");

// Experiment class definition
class Experiment
{
public:
    Experiment ();
    void Configure (std::string phyMode, double rss);
    void Run ();
    uint32_t GetReceivedPackets () const;

private:
    void SetupNodes ();
    void SetupWifi (std::string phyMode, double rss);
    void SetupMobility ();
    void InstallInternetStack ();
    void InstallApplications ();
    void ReceivePacket (Ptr<Socket> socket);

    NodeContainer nodes;
    NetDeviceContainer devices;
    uint32_t receivedPackets;
};

Experiment::Experiment ()
    : receivedPackets (0)
{
}

void
Experiment::Configure (std::string phyMode, double rss)
{
    nodes.Create (2);
    SetupMobility ();
    SetupWifi (phyMode, rss);
    InstallInternetStack ();
    InstallApplications ();
}

void
Experiment::Run ()
{
    Simulator::Run ();
    Simulator::Destroy ();
}

uint32_t
Experiment::GetReceivedPackets () const
{
    return receivedPackets;
}

void
Experiment::SetupNodes ()
{
    nodes.Create (2);
}

void
Experiment::SetupMobility ()
{
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    // Sender at (0,0,0), receiver at (5,0,0)
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (5.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (nodes);
}

void
Experiment::SetupWifi (std::string phyMode, double rss)
{
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211b);

    // Station MAC and AP MAC
    WifiMacHelper mac;
    NetDeviceContainer wifiDevices;

    // Set non-QoS MAC for simplicity
    Ssid ssid = Ssid ("experiment-ssid");
    mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid));
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();

    Ptr<YansWifiChannel> wifiChannel = channel.Create ();
    phy.SetChannel (wifiChannel);

    phy.Set ("RxGain", DoubleValue (0));
    phy.Set ("CcaMode1Threshold", DoubleValue (-80.0));
    phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    // Receiver
    devices = wifi.Install (phy, mac, nodes.Get (0)); // STA
    // Transmitter MAC as AP to avoid association
    mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
    NetDeviceContainer dev = wifi.Install (phy, mac, nodes.Get (1)); // AP
    devices.Add (dev.Get (0));

    // Set WiFi mode and Tx power
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue (phyMode),
                                 "ControlMode", StringValue ("DsssRate1Mbps"));
    phy.Set ("TxPowerStart", DoubleValue (16)); // dBm
    phy.Set ("TxPowerEnd", DoubleValue (16));
    phy.Set ("EnergyDetectionThreshold", DoubleValue (-110.0)); // default

    // Set receiver sensitivity according to tested RSS
    Config::Set ("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/FixedRss", DoubleValue (rss));
}

void
Experiment::InstallInternetStack ()
{
    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    address.Assign (devices);
}

void
Experiment::InstallApplications ()
{
    uint16_t port = 5000;
    // UDP Server (receiver) on node 0
    Ptr<Socket> recvSocket = Socket::CreateSocket (nodes.Get(0), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
    recvSocket->Bind (local);
    recvSocket->SetRecvCallback (MakeCallback (&Experiment::ReceivePacket, this));

    // UDP Client (sender) on node 1
    Ptr<Socket> srcSocket = Socket::CreateSocket (nodes.Get(1), UdpSocketFactory::GetTypeId ());
    Ipv4Address dstAddr = nodes.Get(0)->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
    InetSocketAddress dst = InetSocketAddress (dstAddr, port);

    uint32_t pktSize = 1024;
    uint32_t numPkts = 100;
    Time interPkt = MilliSeconds (10);

    Simulator::ScheduleWithContext (srcSocket->GetNode ()->GetId (), Seconds (1.0), [=](){
        for (uint32_t i = 0; i < numPkts; ++i)
        {
            Simulator::Schedule (interPkt * i, [=]() {
                Ptr<Packet> packet = Create<Packet> (pktSize);
                srcSocket->SendTo (packet, 0, dst);
            });
        }
    });
}

void
Experiment::ReceivePacket (Ptr<Socket> socket)
{
    while (Ptr<Packet> packet = socket->Recv ())
    {
        ++receivedPackets;
    }
}

int
main (int argc, char *argv[])
{
    // Gnuplot setup
    Gnuplot gnuplot ("wifi-rss-vs-packets.eps");
    gnuplot.SetTitle ("802.11b: Packets Received vs. RSS");
    gnuplot.SetTerminal ("postscript eps color enhanced");
    gnuplot.SetLegend ("Received Signal Strength (dBm)", "Packets Received");

    // Test parameters
    std::vector<std::string> phyModes = {
        "DsssRate1Mbps",
        "DsssRate2Mbps",
        "DsssRate5_5Mbps",
        "DsssRate11Mbps"
    };

    std::vector<double> rssValues;
    for (int rss = -100; rss <= -60; rss += 2)
        rssValues.push_back (rss);

    for (const auto& mode : phyModes)
    {
        Gnuplot2dDataset dataset;
        dataset.SetTitle (mode);
        dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

        for (double rss : rssValues)
        {
            Experiment experiment;
            experiment.Configure (mode, rss);
            experiment.Run ();
            dataset.Add (rss, experiment.GetReceivedPackets ());
        }

        gnuplot.AddDataset (dataset);
    }

    std::ofstream plotFile ("wifi-clear-channel.gpl");
    gnuplot.GenerateOutput (plotFile);
    plotFile.close ();

    std::cout << "Results written to wifi-clear-channel.gpl and wifi-rss-vs-packets.eps" << std::endl;
    return 0;
}