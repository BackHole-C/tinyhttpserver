document.addEventListener('DOMContentLoaded', function() {
    const form = document.getElementById('contactForm');
    const messageInput = document.getElementById('message');
    const charCount = document.getElementById('charCount');
    
    messageInput.addEventListener('input', function() {
        const length = this.value.length;
        charCount.textContent = length + '/200';
        if (length > 200) {
            charCount.style.color = '#e74c3c';
            this.value = this.value.substring(0, 200);
            charCount.textContent = '200/200';
        } else {
            charCount.style.color = '#7f8c8d';
        }
    });
    
    form.addEventListener('submit', function(e) {
        e.preventDefault();
        
        const name = document.getElementById('name').value.trim();
        const email = document.getElementById('email').value.trim();
        const message = document.getElementById('message').value.trim();
        
        if (!name) {
            alert('请输入您的姓名');
            return;
        }
        
        if (!email) {
            alert('请输入您的邮箱');
            return;
        }
        
        if (!message) {
            alert('请输入留言内容');
            return;
        }
        
        if (message.length > 200) {
            alert('留言字数不能超过200字');
            return;
        }
        
        fetch('/api/messages', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
            },
            body: 'name=' + encodeURIComponent(name) + 
                  '&email=' + encodeURIComponent(email) + 
                  '&message=' + encodeURIComponent(message)
        })
        .then(response => response.json())
        .then(result => {
            if (result.success) {
                alert('留言发送成功！');
                form.reset();
                charCount.textContent = '0/200';
                loadMessages();
            } else {
                alert(result.error || '发送失败，请重试');
            }
        })
        .catch(error => {
            console.error('Error:', error);
            alert('发送失败，请重试');
        });
    });

    loadMessages();

    const navLinks = document.querySelectorAll('.nav a');
    
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            const targetId = this.getAttribute('href');
            const targetElement = document.querySelector(targetId);
            
            if (targetElement) {
                targetElement.scrollIntoView({
                    behavior: 'smooth'
                });
            }
        });
    });

    window.addEventListener('scroll', function() {
        const header = document.querySelector('.header');
        
        if (window.scrollY > 50) {
            header.style.padding = '30px 20px';
            header.style.boxShadow = '0 5px 30px rgba(0, 0, 0, 0.15)';
        } else {
            header.style.padding = '60px 20px';
            header.style.boxShadow = '0 10px 40px rgba(0, 0, 0, 0.1)';
        }
    });
});

function loadMessages() {
    const messageList = document.getElementById('messageList');
    
    fetch('/api/messages')
    .then(response => response.json())
    .then(messages => {
        if (messages.length === 0) {
            messageList.innerHTML = '<div class="message-empty">暂无留言，快来发表第一条吧！</div>';
            return;
        }
        
        let html = '';
        messages.forEach(msg => {
            html += `
                <div class="message-card">
                    <div class="message-header">
                        <span class="message-name">${escapeHtml(msg.name)}</span>
                        <span class="message-time">${msg.created_at}</span>
                    </div>
                    <div class="message-email">${escapeHtml(msg.email)}</div>
                    <div class="message-content">${escapeHtml(msg.content)}</div>
                </div>
            `;
        });
        
        messageList.innerHTML = html;
    })
    .catch(error => {
        console.error('Error loading messages:', error);
        messageList.innerHTML = '<div class="message-empty">加载失败，请稍后重试</div>';
    });
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}