#include "ns3/bridge-helper.h"
#include "ns3/command-line.h"
#include "ns3/csma-helper.h"
#include "ns3/double.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/rectangle.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/bridge-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    uint32_t nWifis = 2;
    uint32_t nStas = 2;
    bool sendIp = true;
    bool writeMobility = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nWifis", "Number of wifi networks", nWifis);
    cmd.AddValue("nStas", "Number of stations per wifi network", nStas);
    cmd.AddValue("SendIp", "Send Ipv4 or raw packets", sendIp);
    cmd.AddValue("writeMobility", "Write mobility trace", writeMobility);
    cmd.Parse(argc, argv);

    NodeContainer backboneNodes;
    backboneNodes.Create(nWifis);

    CsmaHelper csma;
    NetDeviceContainer backboneDevices = csma.Install(backboneNodes);

    InternetStackHelper stack;
    stack.Install(backboneNodes);

    Ipv4AddressHelper ip;
    ip.SetBase("192.168.0.0", "255.255.255.0");

    typedef std::vector<NodeContainer> NodeContainerVec;
    typedef std::vector<NetDeviceContainer> NetDeviceContainerVec;
    typedef std::vector<Ipv4InterfaceContainer> Ipv4InterfaceContainerVec;

    NodeContainerVec staNodes;
    NetDeviceContainerVec staDevices;
    NetDeviceContainerVec apDevices;
    Ipv4InterfaceContainerVec staInterfaces;
    Ipv4InterfaceContainerVec apInterfaces;

    double wifiX = 0.0;

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    for (uint32_t i = 0; i < nWifis; ++i)
    {
        std::ostringstream oss;
        oss << "wifi-default-" << i;
        Ssid ssid = Ssid(oss.str());

        NodeContainer stas;
        stas.Create(nStas);

        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(wifiX),
                                     "MinY", DoubleValue(0.0),
                                     "DeltaX", DoubleValue(5.0),
                                     "DeltaY", DoubleValue(5.0),
                                     "GridWidth", UintegerValue(1),
                                     "LayoutType", StringValue("RowFirst"));

        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Mode", StringValue("Time"),
                                 "Time", StringValue("2s"),
                                 "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                                 "Bounds", RectangleValue(Rectangle(wifiX, wifiX + 5.0, 0.0, (nStas + 1) * 5.0)));
        mobility.Install(stas);

        stack.Install(stas);

        YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(wifiChannel.Create());

        WifiHelper wifi;
        WifiMacHelper wifiMac;
        wifi.SetStandard(WIFI_STANDARD_80211g);

        wifiMac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid));
        NetDeviceContainer staDev = wifi.Install(wifiPhy, wifiMac, stas);

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(backboneNodes.Get(i));

        wifiMac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
        NetDeviceContainer apDev = wifi.Install(wifiPhy, wifiMac, backboneNodes.Get(i));

        BridgeHelper bridge;
        NetDeviceContainer bridgeDev = bridge.Install(backboneNodes.Get(i), NetDeviceContainer(apDev, backboneDevices.Get(i)));
        Ipv4InterfaceContainer apIf = ip.Assign(bridgeDev);

        Ipv4InterfaceContainer staIf = ip.Assign(staDev);

        staNodes.push_back(stas);
        staDevices.push_back(staDev);
        apDevices.push_back(apDev);
        apInterfaces.push_back(apIf);
        staInterfaces.push_back(staIf);

        wifiX += 20.0;
    }

    Address dest;
    std::string protocol;
    if (sendIp)
    {
        dest = InetSocketAddress(staInterfaces[1].GetAddress(1), 1025);
        protocol = "ns3::UdpSocketFactory";
    }
    else
    {
        PacketSocketAddress tmp;
        tmp.SetSingleDevice(staDevices[0].Get(0)->GetIfIndex());
        tmp.SetPhysicalAddress(staDevices[1].Get(0)->GetAddress());
        tmp.SetProtocol(0x807);
        dest = tmp;
        protocol = "ns3::PacketSocketFactory";
    }

    OnOffHelper onoff(protocol, dest);
    onoff.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer apps = onoff.Install(staNodes[0].Get(0));
    apps.Start(Seconds(0.5));
    apps.Stop(Seconds(3.0));

    for (uint32_t i = 0; i < apDevices.size(); ++i)
    {
        wifiPhy.EnablePcap("wifi-wired-bridging", apDevices[i]);
    }

    if (writeMobility)
    {
        AsciiTraceHelper ascii;
        MobilityHelper::EnableAsciiAll(ascii.CreateFileStream("wifi-wired-bridging.mob"));
    }

    Simulator::Stop(Seconds(5.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}