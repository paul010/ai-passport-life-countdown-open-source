// Original low-resolution game art. No third-party logos or source imagery.
import {writeFileSync} from 'node:fs';
const palette = {'.':null, k:0x524178, p:0x9B7AF7, l:0xCDB7FF,
  w:0xFFFFFF, b:0x284B72, g:0x4CA07A, a:0xA1EBD2, o:0xF0B672};
const monsters = [
 '...k......k...', '...k......k...', '...kkkkkkkk...', '..kppppppppk..',
 '..kplpppplpk..', '..kpwppppwpk..', '.kkpbbppbbpkk.', '.kppppppppppk.',
 '.kpppwwwwpppk.', '..kppppppppk..', '...kkkkkkkk...', '...kk....kk...'
];
const tipo = ['.............','...kkkkkkk...','..kaaaaaaak..','..kaabba aak.'.replace(' ', 'a'),
 '..kaaaaaaak..','..kaawwwaak..','...kkkkkkk...','....koook....','...kaaaaak...',
 '..kkaaaaakk..','...kkkkkkk...','....k...k....'];
const reed = ['......g......','.....ggg.....','....ggagg....','...ggaaagg...',
 '..ggaaaaagg..','..ggagagagg..','...ggagagg...','....ggagg....','.....ggg.....',
 '......g......','....g.g.g....','...ggggggg...'];
function bitmap(rows,scale,pal) {
 const width=Math.max(...rows.map(x=>x.length))*scale,height=rows.length*scale;
 const color=[],alpha=[];
 for(let y=0;y<height;y++)for(let x=0;x<width;x++) {
  const c=pal[rows[Math.floor(y/scale)][Math.floor(x/scale)]??'.'];
  const v=c===null||c===undefined?0:((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31);
  color.push(v&255,v>>8);alpha.push(c===null||c===undefined?0:255);
 }
 return {width,height,bytes:[...color,...alpha]};
}
let code='#include "quest_sprites.h"\n\n';
function add(name,rows,scale,pal=palette) {
 const a=bitmap(rows,scale,pal);
 code+=`static const uint8_t ${name}_data[] = {\n`;
 for(let i=0;i<a.bytes.length;i+=24)code+='    '+a.bytes.slice(i,i+24).map(n=>'0x'+n.toString(16).padStart(2,'0')).join(',')+',\n';
 code+='};\n';
 return `{.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565A8,.w=${a.width},.h=${a.height},.stride=${a.width*2}},.data_size=sizeof(${name}_data),.data=${name}_data}`;
}
const td=add('tipo',tipo,2),rd=add('reed',reed,2);
const ds=[palette,{...palette,p:0xE8A873,l:0xF8D6A4},{...palette,p:0x5ABAA5,l:0xB0E6C7}]
 .map((p,i)=>add('monster'+i,monsters,3,p));
code+='const lv_image_dsc_t quest_tipo_image = '+td+';\n';
code+='const lv_image_dsc_t quest_reed_image = '+rd+';\n';
code+='const lv_image_dsc_t quest_monster_images[3] = {\n'+ds.join(',\n')+'\n};\n';
writeFileSync(new URL('../main/quest_sprites.c',import.meta.url),code);
