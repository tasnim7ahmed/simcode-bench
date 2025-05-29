#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MultiApWifiSimulation");

static std::map<uint32_t, uint64_t> staBytesSent;

void TxTrace (Ptr<const Packet> packet, Mac48Address, Mac48Address)
{
    Ptr<Node> node = NodeList::GetNode (Simulator::GetContext ());
    uint32_t nodeId = node->GetId ();
    staBytesSent[nodeId] += packet->GetSize ();
}

int main (int argc, char *argv[])
{
    uint32_t nAp = 2;
    uint32_t nStaPerAp = 3;
    uint32_t packetSize = 1024;
    double simulationTime = 10.0;
    double dataRateMbps = 5.0;

    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    NodeContainer wifiApNodes;
    wifiApNodes.Create (nAp);

    NodeContainer wifiStaNodes;
    for (uint32_t i = 0; i < nStaPerAp * nAp; ++i)
        wifiStaNodes.Create (1);

    // Separate containers for station groups for each AP
    std::vector<NodeContainer> apStaGroups;
    for (uint32_t i = 0; i < nAp; ++i)
    {
        NodeContainer group;
        for (uint32_t j = 0; j < nStaPerAp; ++j)
        {
            group.Add (wifiStaNodes.Get (i * nStaPerAp + j));
        }
        apStaGroups.push_back (group);
    }

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    // MAC helpers and SSID setups
    std::vector<Ssid> ssids;
    std::vector<NetDeviceContainer> staDevicesVec;
    std::vector<NetDeviceContainer> apDevicesVec;

    for (uint32_t i = 0; i < nAp; ++i)
    {
        std::ostringstream ssidString;
        ssidString << "wifi-ap-" << i;
        Ssid ssid = Ssid (ssidString.str ());
        ssids.push_back (ssid);

        WifiMacHelper macSta, macAp;
        macSta.SetType ("ns3::StaWifiMac",
                        "Ssid", SsidValue (ssid),
                        "ActiveProbing", BooleanValue (false));
        NetDeviceContainer staDevices = wifi.Install (phy, macSta, apStaGroups[i]);
        staDevicesVec.push_back (staDevices);

        macAp.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));
        NetDeviceContainer apDevices = wifi.Install (phy, macAp, wifiApNodes.Get (i));
        apDevicesVec.push_back (apDevices);
    }

    // Mobility
    MobilityHelper mobilityAp;
    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator> ();
    apPosAlloc->Add (Vector (0.0, 0.0, 0.0));
    apPosAlloc->Add (Vector (20.0, 0.0, 0.0));
    mobilityAp.SetPositionAllocator (apPosAlloc);
    mobilityAp.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobilityAp.Install (wifiApNodes);

    MobilityHelper mobilitySta;
    Ptr<ListPositionAllocator> staPosAlloc = CreateObject<ListPositionAllocator> ();
    for (uint32_t i = 0; i < nAp; ++i)
    {
        for (uint32_t j = 0; j < nStaPerAp; ++j)
        {
            staPosAlloc->Add (Vector (i*20 + 5.0 + 2.0*j, 5.0, 0.0));
        }
    }
    mobilitySta.SetPositionAllocator (staPosAlloc);
    mobilitySta.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobilitySta.Install (wifiStaNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (wifiApNodes);
    stack.Install (wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> apIfaces, staIfacesVec;
    for (uint32_t i = 0; i < nAp; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase (subnet.str ().c_str (), "255.255.255.0");
        Ipv4InterfaceContainer staIfaces = address.Assign (staDevicesVec[i]);
        Ipv4InterfaceContainer apIfacesTemp = address.Assign (apDevicesVec[i]);
        staIfacesVec.push_back (staIfaces);
        apIfaces.push_back (apIfacesTemp);
    }

    // UDP Server(s): One per AP, on AP node -- can also use just one server for all
    uint16_t port = 9000;
    std::vector<ApplicationContainer> serverApps;
    for (uint32_t i = 0; i < nAp; ++i)
    {
        UdpServerHelper server (port);
        ApplicationContainer apps = server.Install (wifiApNodes.Get (i));
        apps.Start (Seconds (1.0));
        apps.Stop (Seconds (simulationTime));
        serverApps.push_back (apps);
    }

    // UDP Client(s): Each STA sends to its AP's server
    for (uint32_t i = 0; i < nAp; ++i)
    {
        for (uint32_t j = 0; j < nStaPerAp; ++j)
        {
            auto staNode = apStaGroups[i].Get (j);
            Ipv4Address apIp = apIfaces[i].GetAddress (0);

            UdpClientHelper client (apIp, port);
            client.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
            client.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
            client.SetAttribute ("PacketSize", UintegerValue (packetSize));

            ApplicationContainer clientApp = client.Install (staNode);
            clientApp.Start (Seconds (2.0));
            clientApp.Stop (Seconds (simulationTime));
            
            // Trace at the Wi-Fi device MAC Tx for this STA for bytes sent
            for (uint32_t d = 0; d < staNode->GetNDevices (); ++d)
            {
                Ptr<NetDevice> dev = staNode->GetDevice (d);
                Ptr<WifiNetDevice> wifiDev = dev->GetObject<WifiNetDevice> ();
                if (wifiDev)
                {
                    wifiDev->GetMac ()->TraceConnectWithoutContext ("Tx", MakeCallback (&TxTrace));
                }
            }
        }
    }

    // Enable pcap and ASCII tracing
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    for (uint32_t i = 0; i < nAp; ++i)
    {
        std::ostringstream oss;
        oss << "ap" << i;
        phy.EnablePcap (oss.str (), apDevicesVec[i], true);
        phy.EnableAscii ("ap-phy.tr", apDevicesVec[i]);
    }

    for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
        std::ostringstream oss;
        oss << "sta" << i;
        phy.EnablePcap (oss.str (), wifiStaNodes.Get(i)->GetDevice (0), true);
        phy.EnableAscii ("sta-phy.tr", wifiStaNodes.Get(i)->GetDevice (0));
    }
    
    Simulator::Stop (Seconds (simulationTime + 1.0));

    Simulator::Run ();

    // Print total data sent by each STA
    std::cout << "Total data sent by each station (bytes):\n";
    for (uint32_t i = 0; i < wifiStaNodes.GetN (); ++i)
    {
        uint32_t nodeId = wifiStaNodes.Get (i)->GetId ();
        uint64_t bytes = 0;
        if (staBytesSent.find (nodeId) != staBytesSent.end ())
            bytes = staBytesSent[nodeId];
        std::cout << "  Station " << i << " (NodeId " << nodeId << "): " << bytes << " bytes\n";
    }

    Simulator::Destroy ();
    return 0;
}